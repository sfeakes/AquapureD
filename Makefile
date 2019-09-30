
# define the C compiler to use
CC = gcc

#LIBS := -lpthread -lm
#LIBS := -lpthread -lwebsockets
LIBS := -lm

# debug of not
#DBG = -g -O0 -fsanitize=address -static-libasan
#DBG = -g
#DBG = -D TESTING
DBG =

# define any compile-time flags
#CFLAGS = -Wall -g -lpthread -lwiringPi -lm -I. 
#CFLAGS = -Wall -g $(LIBS) -I/usr/local/include/ -L/usr/local/lib/
#CFLAGS = -Wall -g $(LIBS) -std=gnu11 -I/nas/data/Development/Raspberry/aqualink/libwebsockets-2.0-stable/lib -L/nas/data/Development/Raspberry/aqualink/libwebsockets-2.0-stable/lib
#CFLAGS = -Wall -g $(LIBS)
#CFLAGS = -Wall -g $(LIBS) -std=gnu11
CFLAGS = -Wall $(DBG) $(LIBS) -D MG_DISABLE_MD5 -D MG_DISABLE_HTTP_DIGEST_AUTH -D MG_DISABLE_MD5 -D MG_DISABLE_JSON_RPC
#CFLAGS = -Wall $(DBG) $(LIBS) -D MG_DISABLE_MQTT -D MG_DISABLE_MD5 -D MG_DISABLE_HTTP_DIGEST_AUTH -D MG_DISABLE_MD5 -D MG_DISABLE_JSON_RPC

#INCLUDES = -I/nas/data/Development/Raspberry/aqualink/aqualinkd
#INCLUDES = -I./ -I../ -I./aquapure/
INCLUDES = -I./

# Add inputs and outputs from these tool invocations to the build variables 

# define the C source files
#SRCS = aqualinkd.c utils.c config.c aq_serial.c init_buttons.c aq_programmer.c net_services.c json_messages.c mongoose.c

#SL_SRC = serial_logger.c aq_serial.c utils.c
#AL_SRC = aquapure_logger.c aq_serial.c utils.c
SRCS = aquapure.c ap_net_services.c ap_config.c aq_serial.c utils.c mongoose.c json_messages.c config.c
#SRCS = aq_serial.c utils.c mongoose.c json_messages.c config.c aquapured/ap_net_services.c aquapured/ap_config.c aquapured/aquapure.c

OBJS = $(SRCS:.c=.o)

#SL_OBJS = $(SL_SRC:.c=.o)
#AL_OBJS = $(AL_SRC:.c=.o)
#AP_OBJS = $(AP_SRC:.c=.o)

# define the executable file
MAIN = ./release/aquapured

#SLOG = ./release/serial_logger
#AQUAPURELOG = ./release/aquapure_logger
#AQUAPURED = ./release/aquapured

all:    $(MAIN) 
  @echo: $(MAIN) have been compiled

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)


# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN) $(MAIN_U)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

install: $(MAIN)
	./release/install.sh
  

# All Target
#all: aqualinkd
#
# Tool invocations
#aqualinkd: $(OBJS) $(USER_OBJS)
#	@echo 'Building target: $@'
#	@echo 'Invoking: GCC C Linker'
#	gcc -L/home/perry/workspace/libwebsockets/Debug -pg -o"aqualinkd" $(OBJS) $(USER_OBJS) $(LIBS)
#	@echo 'Finished building target: $@'
#	@echo ' '
#
# Other Targets
#clean:
#	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) aqualinkd
#	-@echo ' '
#
#.PHONY: all clean dependents
#.SECONDARY:
#
#-include ../makefile.targets
