################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../src/TskData/TskData.cc 

CC_DEPS += \
./src/TskData/TskData.d 

OBJS += \
./src/TskData/TskData.o 


# Each subdirectory must supply rules for building sources it contributes
src/TskData/%.o: ../src/TskData/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: ARM v7 g++ compiler'
	arm-none-eabi-g++ -Wall -O0 -g3 -c -fmessage-length=0 -MT"$@" -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -IZ:/vitis_poject/202409261/tsktcp/export/tsktcp/sw/tsktcp/freertos10_xilinx_domain/bspinclude/include -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


