include ../Makefile.tests_common

BOARD_INSUFFICIENT_MEMORY := chronos msb-430 msb-430h nucleo32-f031 \
                             nucleo32-f042 nucleo32-l031 nucleo-f030 \
                             nucleo-f334 nucleo-l053 stm32f0discovery \
                             telosb z1

USEMODULE += sntp
USEMODULE += gnrc_sock_udp
USEMODULE += gnrc_ipv6_default
USEMODULE += auto_init_gnrc_netif
USEMODULE += gnrc_netdev_default
USEMODULE += shell
USEMODULE += shell_commands

include $(RIOTBASE)/Makefile.include
