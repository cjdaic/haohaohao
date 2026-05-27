################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../src/XilPeriph/Axi4lFifoIF.cc \
../src/XilPeriph/Eth.cc \
../src/XilPeriph/XilAxiDma.cc \
../src/XilPeriph/XilIntc.cc \
../src/XilPeriph/XilPsDma.cc 

CC_DEPS += \
./src/XilPeriph/Axi4lFifoIF.d \
./src/XilPeriph/Eth.d \
./src/XilPeriph/XilAxiDma.d \
./src/XilPeriph/XilIntc.d \
./src/XilPeriph/XilPsDma.d 

OBJS += \
./src/XilPeriph/Axi4lFifoIF.o \
./src/XilPeriph/Eth.o \
./src/XilPeriph/XilAxiDma.o \
./src/XilPeriph/XilIntc.o \
./src/XilPeriph/XilPsDma.o 


# Each subdirectory must supply rules for building sources it contributes
src/XilPeriph/%.o: ../src/XilPeriph/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: ARM v7 g++ compiler'
	arm-none-eabi-g++ -Wall -O0 -g3 -c -fmessage-length=0 -MT"$@" -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -IZ:/vitis_poject/202409261/tsktcp/export/tsktcp/sw/tsktcp/freertos10_xilinx_domain/bspinclude/include -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


