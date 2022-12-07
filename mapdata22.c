/*
 * mapdata.c
 *
 * based on mapd (https://github.com/Osolemio/malina)
 *
 * ver 0.1. * asdx
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

/***************************/

#define to_read 0x72
#define to_write 0x77
#define size_of_buffer 560

unsigned char Buffer[size_of_buffer];
unsigned char sum;

int put_char(int fd, unsigned char a)
{
    unsigned char a1;
    unsigned int h=0;

    if (write(fd,&a,1)!=1) {
        return -1;
    }
    if (read(fd,&a1,1)!=1) {
        return -1;
    }

    if (a!=a1) {
        do {
            read(fd,&a1,1);
        } while (a!=a1 && h++<20);
    } // tuning to the stream if there are omitten bytes
    if (a!=a1) {
        return -1;
    }
    return 0;
}

void code_DB (int fd, unsigned char a)
{
    if (a == '\n') {
        sum += 0xDB;
        if (put_char (fd, 0xDB) != 0) {
            return;
        }
        sum += 0xDC;
        if (put_char (fd, 0xDC) != 0) {
            return;
        }
    } else if (a == 0xDB) {
        sum += 0xDB;
        if (put_char (fd, 0xDB) != 0) {
            return;
        }
        sum += 0xDD;
        if (put_char (fd, 0xDD) != 0) {
            return;
        }
    } else {
        sum += a;
        put_char (fd, a);
        return;
    }
}

void send_command (unsigned char command, int fd, unsigned short addr, unsigned short page)
{
    unsigned char a[4];
    short i;
    sum = 0;
    a[0] = command;
    a[1] = page;
    a[2] = addr >> 8;
    a[3] = addr & 0xFF;
    for (i = 0; i < 4; i++) {
        code_DB (fd, a[i]);
    }
    if (command == to_write) {
        for (i = 0; i <= page; i++) {
            code_DB (fd, Buffer[i]);
        }
    }
    sum = 0xff - sum;
    sum++;
    if (put_char (fd, sum) != 0) {
        return;
    }

    if (sum != '\n') {
        put_char (fd, '\n');
    }
}

void decode_answer (int i)
{
    int c, c1;
    for (c = 1; c <= i; c++) {
        if (Buffer[c] == 0xDB && Buffer[c + 1] == 0xDC) {
            Buffer[c] = 0x0A;--i;
            for (c1 = c; c1 < (i-1); c1++) {
                Buffer[c1 + 1] = Buffer[c1 + 2];
            }
        } else if (Buffer[c] == 0xDB && Buffer[c + 1] == 0xDD) {
            Buffer[c] = 0xDB;--i;
            for (c1 = c; c1 < (i-1); c1++) {
                Buffer[c1 + 1] = Buffer[c1 + 2];
            }
        }
    }
}

int read_answer (int fd)
{
    int i = 0, c = 0;
    unsigned char sum_r = 0;
    if (read (fd, &Buffer, 1) != 1) {
        return -1;
    }
    do {
        if (write (fd, &Buffer[i], 1) != 1) {
            return -1;
        }
        if (read (fd, &Buffer[++i], 1) != 1) {
            return -1;
        }
    } while (Buffer[i] != 0x0A && i < 560);

    if (write (fd, &Buffer[i], 1) != 1) {
        return -1;
    }
    if (Buffer[0] == 0x65) {
      return -1;
    }

    if (Buffer[0] == 0x6f) {
        for (c = 0; c < (i - 1); c++) {
            sum_r += Buffer[c];
        }
        sum_r = 0xff - sum_r;
        sum_r++;
        if ((sum_r != 0x0A) && (sum_r != Buffer[i - 1])) {
            return -1;
        }
        if ((sum_r == 0x0A) && (sum_r != Buffer[i])) {
            return -1;
        }
        decode_answer (i);
        return 0;
    }

    return -1;
}




int main(int argc, char* argv[])
{
    int sockfd, portno;
    struct timeval tv;
    struct hostent *server;
    struct sockaddr_in serv_addr;
    struct map_info
    {
        unsigned char _MODE;
        unsigned char _Status_Char;
        float _Uacc;
        unsigned int _Iacc;
        unsigned int _PLoad;
        unsigned char _F_Acc_Over;
        unsigned char _F_Net_Over;
        unsigned int _UNET;
        unsigned char _INET;
        unsigned int _PNET;
        unsigned char _TFNET;
        unsigned char _ThFMAP;
        unsigned char _TFNET_Hz;
        unsigned char _ThFMAP_Hz;
        unsigned int _UOUTmed;
        unsigned char _TFNET_Limit;
        unsigned int _UNET_Limit;
        unsigned char _RSErrSis;
        unsigned char _RSErrJobM;
        unsigned char _RSErrJob;
        unsigned char _RSWarning;
        signed char _Temp_Grad0;
        signed char _Temp_Grad2;
        float _INET_16_4;
        float _IAcc_med_A_u16;
        unsigned char _Temp_off;
        unsigned long int _E_NET;
        unsigned long int _E_ACC;
        unsigned long int _E_ACC_CHARGE;
        float _Inet_flag;
        float _I_acc_avg;
        float _I_mppt_avg;
        unsigned char _I2C_err;
        char _Temp_Grad1;
        unsigned char _Relay1;
        unsigned char _Relay2;
        unsigned char _Flag_ECO;
        unsigned char _RSErrDop;
        unsigned char _flagUnet2;
    };
    struct map_info map_data;
    struct tm *newtime;
    struct tm tim;
    time_t ltime;

    tv.tv_sec = 4;  /* Timeout */
    tv.tv_usec = 0;  // Not init'ing this can cause strange errors

    if (argc < 3) {
        fprintf(stderr, "Usage %s hostname port\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[2]);

    /* Create a socket point */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        fprintf(stderr, "ERROR: can't open socket\n");
        exit(1);
    }

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    server = gethostbyname(argv[1]);

    if (server == NULL) {
        fprintf(stderr, "ERROR: no such host\n");
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "ERROR: can't connect to %s:%d\n", argv[1], portno);
        exit(1);
    }


    send_command (to_read, sockfd, 0x400, 0xFF);
    if (read_answer (sockfd) == 0) {
        map_data._MODE = Buffer[0x400 - 0x3FF];
        //num_maps=(eeprom[0x155]==255)?1:(eeprom[0x155]+1);

        map_data._UNET = Buffer[0x422 - 0x3FF];
        if (map_data._UNET>0) {
            map_data._UNET += 100;
        }

        // change MAP mode to my extensions
        // map_data._MODE=real_mode(map_data._MODE, eeprom[0x16B], map_data._Flag_ECO, eeprom[0x13C], eeprom[0x13B], map_data._UNET, eeprom[0x58c]);
        map_data._Status_Char = Buffer[0x402 - 0x3FF];

        map_data._Uacc = (Buffer[0x405 - 0x3FF] * 256 + Buffer[0x406 - 0x3FF]);
        map_data._Uacc = map_data._Uacc / 10;
        map_data._Iacc = Buffer[0x408 - 0x3FF];
        map_data._Iacc *= 2;

        map_data._PLoad = Buffer[0x409 - 0x3FF];
        map_data._PLoad *= 100;

        map_data._F_Acc_Over = Buffer[0x41C - 0x3FF];
        map_data._F_Net_Over = Buffer[0x41D - 0x3FF];

        map_data._INET = Buffer[0x423 - 0x3FF];
        map_data._PNET = Buffer[0x424 - 0x3FF];
        map_data._PNET *= 100;

        map_data._TFNET = Buffer[0x425 - 0x3FF];
        map_data._TFNET_Hz = 0;
        if (map_data._TFNET != 0) {
            map_data._TFNET_Hz = 6250 / map_data._TFNET;
        }

        map_data._ThFMAP = Buffer[0x426 - 0x3FF];
        map_data._ThFMAP_Hz = 0;
        if (map_data._ThFMAP != 0) {
            map_data._ThFMAP_Hz = 6250 / map_data._ThFMAP;
        }

        map_data._UOUTmed = Buffer[0x427 - 0x3FF];
        if (map_data._UOUTmed > 0) {
            map_data._UOUTmed += 100;
        }

        map_data._TFNET_Limit = Buffer[0x428 - 0x3FF];
        //if (map_data._TFNET_Limit!=0) map_data._TFNET_Limit= 2500 / map_data._TFNET_Limit;

        map_data._UNET_Limit = Buffer[0x429 - 0x3FF];
        map_data._UNET_Limit += 100;

        map_data._RSErrSis = Buffer[0x42A - 0x3FF];
        map_data._RSErrJobM = Buffer[0x42B - 0x3FF];
        map_data._RSErrJob = Buffer[0x42C - 0x3FF];
        map_data._RSWarning = Buffer[0x2E];

        map_data._Temp_Grad0 = Buffer[0x2F] - 50;
        map_data._Temp_Grad1 = Buffer[0x30] - 50;

        map_data._Temp_Grad2 = Buffer[0x430 - 0x3FF] - 50;

        if (map_data._INET < 16) {
            map_data._INET_16_4 =(float)Buffer[0x32] / 16;
        } else {
            map_data._INET_16_4 =(float)Buffer[0x32] / 4;
        }

        map_data._IAcc_med_A_u16 = (float)Buffer[0x34]*16+(float)Buffer[0x33] / 16;
        map_data._Temp_off = Buffer[0x43C - 0x3FF];
        map_data._E_NET = (Buffer[0x50] * 65536 + Buffer[0x4F] * 256 + Buffer[0x4E]);
        map_data._E_ACC = (Buffer[0x53] * 65536 + Buffer[0x52] * 256 + Buffer[0x51]);
        map_data._E_ACC_CHARGE = (Buffer[0x56] * 65536 + Buffer[0x55] * 256 + Buffer[0x54]);
        map_data._I2C_err = Buffer[0x45A-0x3FF];
        map_data._RSErrDop= Buffer[0x447-0x3FF];

        map_data._Inet_flag= Buffer[0x443-0x3FF]+Buffer[0x444-0x3FF]*256; if (map_data._Inet_flag>32768) map_data._Inet_flag=1; else map_data._Inet_flag=0;

        time (&ltime);
        newtime = localtime (&ltime);
        tim = *newtime;

        fprintf (stdout, "{\"_DATE\":\"%04d-%02d-%02d %02d:%02d:%02d\","
                "\"_MODE\":\"%d\",\"_Status_Char\":\"%d\",\"_Uacc\":\"%.1f\",\"_Iacc\":\"%i\","
                "\"_PLoad\":\"%i\",\"_F_Acc_Over\":\"%d\",\"_F_Net_Over\":\"%d\","
                "\"_UNET\":\"%i\",\"_INET\":\"%d\",\"_PNET\":\"%i\",\"_TFNET\":\"%d\",\"_TFNET_Hz\":\"%d\", \"_ThFMAP\":\"%d\", \"_ThFMAP_Hz\":\"%d\","
                "\"_UOUTmed\":\"%i\",\"_TFNET_Limit\":\"%d\",\"_UNET_Limit\":\"%i\","
                "\"_RSErrSis\":\"%d\",\"_RSErrJobM\":\"%d\",\"_RSErrJob\":\"%d\",\"_RSWarning\":\"%d\","
                "\"_Temp_Grad0\":\"%d\",\"_Temp_Grad2\":\"%d\",\"_INET_16_4\":\"%.1f\",\"_IAcc_med_A_u16\":\"%.1f\","
                "\"_Temp_off\":\"%d\",\"_E_NET\":\"%ld\",\"_E_ACC\":\"%ld\",\"_E_ACC_CHARGE\":\"%ld\",\"_Inet_flag\":\"%.1f\","
                "\"_I_acc_avg\":\"%.1f\",\"_I_mppt_avg\":\"%.1f\",\"_I2C_Err\":\"%d\",\"_Temp_Grad1\":\"%d\","
                "\"_Relay1\":\"%d\",\"_Relay2\":\"%d\",\"_Flag_ECO\":\"%d\",\"_RSErrDop\":\"%d\",\"_flagUnet2\":\"%d\"}\n",
            tim.tm_year + 1900, tim.tm_mon + 1, tim.tm_mday,tim.tm_hour, tim.tm_min, tim.tm_sec,
            map_data._MODE, map_data._Status_Char, map_data._Uacc, map_data._Iacc,
            map_data._PLoad, map_data._F_Acc_Over, map_data._F_Net_Over,
            map_data._UNET, map_data._INET, map_data._PNET, map_data._TFNET, map_data._TFNET_Hz, map_data._ThFMAP, map_data._ThFMAP_Hz,
            map_data._UOUTmed, map_data._TFNET_Limit, map_data._UNET_Limit,
            map_data._RSErrSis, map_data._RSErrJobM, map_data._RSErrJob, map_data._RSWarning,
            map_data._Temp_Grad0, map_data._Temp_Grad2, map_data._INET_16_4, map_data._IAcc_med_A_u16, 
            map_data._Temp_off, map_data._E_NET, map_data._E_ACC, map_data._E_ACC_CHARGE, map_data._Inet_flag, 
            map_data._I_acc_avg, map_data._I_mppt_avg, map_data._I2C_err, map_data._Temp_Grad1, 
            map_data._Relay1, map_data._Relay2, map_data._Flag_ECO, map_data._RSErrDop, map_data._flagUnet2);

    } else {
        fprintf(stderr, "ERROR: failed get data from %s:%d\n", argv[1], portno);
    }

    bzero (Buffer, size_of_buffer);
    close(sockfd);
    exit(0);
}
