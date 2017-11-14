/*
 * Copyright © 2008-2010 Stéphane Raimbault <stephane.raimbault@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <modbus/modbus.h>

#include "unit-test.h"

enum {
    TCP,
    TCP_PI,
    RTU
};

int main(int argc, char *argv[])
{
    struct timeval t_start,t_end;
    uint8_t *tab_rp_bits;
    uint16_t *tab_rp_registers;
    uint16_t *tab_rp_registers_bad;
    modbus_t *ctx;
    int i;
    uint8_t value;
    int nb_points;
    int rc;
    float real;
    uint32_t ireal;
    struct timeval old_response_timeout;
    struct timeval response_timeout;
    int use_backend;

    if (argc > 1) {
        if (strcmp(argv[1], "tcp") == 0) {
            use_backend = TCP;
	} else if (strcmp(argv[1], "tcppi") == 0) {
            use_backend = TCP_PI;
        } else if (strcmp(argv[1], "rtu") == 0) {
            use_backend = RTU;
        } else {
            printf("Usage:\n  %s [tcp|tcppi|rtu] - Modbus client for unit testing\n\n", argv[0]);
            exit(1);
        }
    } else {
        /* By default */
        use_backend = TCP;
    }

    if (use_backend == TCP) {
        ctx = modbus_new_tcp("192.168.0.149", 1502);
    } else if (use_backend == TCP_PI) {
        ctx = modbus_new_tcp_pi("::1", "1502");
    } else {
        ctx = modbus_new_rtu("/dev/ttyUSB1", 115200, 'N', 8, 1);
    }
    if (ctx == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        return -1;
    }
    modbus_set_debug(ctx, TRUE);
    modbus_set_error_recovery(ctx,
                              MODBUS_ERROR_RECOVERY_LINK |
                              MODBUS_ERROR_RECOVERY_PROTOCOL);

    if (use_backend == RTU) {
          modbus_set_slave(ctx, SERVER_ID);
    }

    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Connection failed: %s\n",
                modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    /* Allocate and initialize the memory to store the bits */
    nb_points = (UT_BITS_NB > UT_INPUT_BITS_NB) ? UT_BITS_NB : UT_INPUT_BITS_NB;
    tab_rp_bits = (uint8_t *) malloc(nb_points * sizeof(uint8_t));
    memset(tab_rp_bits, 0, nb_points * sizeof(uint8_t));

    /* Allocate and initialize the memory to store the registers */
    nb_points = (UT_REGISTERS_NB > UT_INPUT_REGISTERS_NB) ?
        UT_REGISTERS_NB : UT_INPUT_REGISTERS_NB;
    tab_rp_registers = (uint16_t *) malloc(nb_points * sizeof(uint16_t));
    memset(tab_rp_registers, 0, nb_points * sizeof(uint16_t));

    printf("** 单元测试 **\n");

    printf("\n线圈读写测试:\n");

    /** COIL BITS **/
    /* Single */
    printf("[1] ModBus单线圈（位）写: ");
    gettimeofday(&t_start,NULL);  
    rc = modbus_write_bit(ctx, UT_BITS_ADDRESS, ON);
    gettimeofday(&t_end,NULL);  
    printf("访问耗时:%ld us\n",t_end.tv_usec - t_start.tv_usec);
    if (rc == 1) {
        printf("成功\n");
    } else {
        printf("FAILED\n");
        goto close;
    }
    printf("线圈读回: ");
    gettimeofday(&t_start,NULL);  
    rc = modbus_read_bits(ctx, UT_BITS_ADDRESS, 1, tab_rp_bits);
    gettimeofday(&t_end,NULL);  
    printf("访问耗时:%ld us\n",t_end.tv_usec - t_start.tv_usec);
    if (rc != 1) {
        printf("FAILED (nb points %d)\n", rc);
        goto close;
    }

    if (tab_rp_bits[0] != ON) {
        printf("FAILED (%0X = != %0X)\n", tab_rp_bits[0], ON);
        goto close;
    }
    printf("成功\n");
    /* End single */

    {
        uint8_t tab_value[UT_BITS_NB];
        printf("[2] ModBus多线圈（位）写: ");
        gettimeofday(&t_start,NULL);  
        modbus_set_bits_from_bytes(tab_value, 0, UT_BITS_NB, UT_BITS_TAB);
        rc = modbus_write_bits(ctx, UT_BITS_ADDRESS,
                               UT_BITS_NB, tab_value);
        gettimeofday(&t_end,NULL);  
        printf("访问耗时:%ld us\n",t_end.tv_usec - t_start.tv_usec);
        if (rc == UT_BITS_NB) {
            printf("成功\n");
        } else {
            printf("FAILED\n");
            goto close;
        }
    }
    printf("线圈读回: ");
        gettimeofday(&t_start,NULL);  
    rc = modbus_read_bits(ctx, UT_BITS_ADDRESS, UT_BITS_NB, tab_rp_bits);
        gettimeofday(&t_end,NULL); 
        printf("访问耗时:%ld us\n",t_end.tv_usec - t_start.tv_usec);
    if (rc != UT_BITS_NB) {
        printf("FAILED (nb points %d)\n", rc);
        goto close;
    }

    i = 0;
    nb_points = UT_BITS_NB;
    while (nb_points > 0) {
        int nb_bits = (nb_points > 8) ? 8 : nb_points;

        value = modbus_get_byte_from_bits(tab_rp_bits, i*8, nb_bits);
        if (value != UT_BITS_TAB[i]) {
            printf("FAILED (%0X != %0X)\n", value, UT_BITS_TAB[i]);
            goto close;
        }

        nb_points -= nb_bits;
        i++;
    }
    printf("成功\n");
    /* End of multiple bits */

    /** DISCRETE INPUTS **/
    printf("[3] ModBus读离散量输入（位）: ");
        gettimeofday(&t_start,NULL);  
    rc = modbus_read_input_bits(ctx, UT_INPUT_BITS_ADDRESS,
                                UT_INPUT_BITS_NB, tab_rp_bits);

        gettimeofday(&t_end,NULL); 
        printf("访问耗时:%ld us\n",t_end.tv_usec - t_start.tv_usec);

    if (rc != UT_INPUT_BITS_NB) {
        printf("FAILED (nb points %d)\n", rc);
        goto close;
    }

    i = 0;
    nb_points = UT_INPUT_BITS_NB;
    while (nb_points > 0) {
        int nb_bits = (nb_points > 8) ? 8 : nb_points;

        value = modbus_get_byte_from_bits(tab_rp_bits, i*8, nb_bits);
        if (value != UT_INPUT_BITS_TAB[i]) {
            printf("FAILED (%0X != %0X)\n", value, UT_INPUT_BITS_TAB[i]);
            goto close;
        }

        nb_points -= nb_bits;
        i++;
    }
    printf("成功\n");

    /** HOLDING REGISTERS **/
    printf("\n寄存器读写测试:\n");

    /* Single register */
        gettimeofday(&t_start,NULL);  
    printf("[4] ModBus单寄存器写: ");
    rc = modbus_write_register(ctx, UT_REGISTERS_ADDRESS, 0x1234);
        gettimeofday(&t_end,NULL); 
        printf("访问耗时:%ld us\n",t_end.tv_usec - t_start.tv_usec);
    if (rc == 1) {
        printf("成功\n");
    } else {
        printf("FAILED\n");
        goto close;
    }
    printf("寄存器读回: ");
        gettimeofday(&t_start,NULL); 
    rc = modbus_read_registers(ctx, UT_REGISTERS_ADDRESS,
                               1, tab_rp_registers);
        gettimeofday(&t_end,NULL); 
        printf("访问耗时:%ld us\n",t_end.tv_usec - t_start.tv_usec);
    if (rc != 1) {
        printf("FAILED (nb points %d)\n", rc);
        goto close;
    }

    if (tab_rp_registers[0] != 0x1234) {
        printf("FAILED (%0X != %0X)\n",
               tab_rp_registers[0], 0x1234);
        goto close;
    }
    printf("成功\n");
    /* End of single register */

    /* Many registers */
    printf("[5] ModBus多寄存器写: ");
        gettimeofday(&t_start,NULL); 
    rc = modbus_write_registers(ctx, UT_REGISTERS_ADDRESS,
                                UT_REGISTERS_NB, UT_REGISTERS_TAB);
        gettimeofday(&t_end,NULL); 
        printf("访问耗时:%ld us\n",t_end.tv_usec - t_start.tv_usec);

    if (rc == UT_REGISTERS_NB) {
        printf("OK\n");
    } else {
        printf("FAILED\n");
        goto close;
    }
    printf("寄存器读回: ");
        gettimeofday(&t_start,NULL); 
    rc = modbus_read_registers(ctx, UT_REGISTERS_ADDRESS,
                               UT_REGISTERS_NB, tab_rp_registers);
        gettimeofday(&t_end,NULL); 
        printf("访问耗时:%ld us\n",t_end.tv_usec - t_start.tv_usec);

    if (rc != UT_REGISTERS_NB) {
        printf("FAILED (nb points %d)\n", rc);
        goto close;
    }

    for (i=0; i < UT_REGISTERS_NB; i++) {
        if (tab_rp_registers[i] != UT_REGISTERS_TAB[i]) {
            printf("FAILED (%0X != %0X)\n",
                   tab_rp_registers[i],
                   UT_REGISTERS_TAB[i]);
            goto close;
        }
    }
    printf("成功\n");
    printf("\n所有测试通过\n");

close:
    /* Free the memory */
    free(tab_rp_bits);
    free(tab_rp_registers);

    /* Close the connection */
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}
