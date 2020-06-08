################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../custom/config/custom_sys_cfg.c \
../custom/config/sys_config.c 

OBJS += \
./custom/config/custom_sys_cfg.o \
./custom/config/sys_config.o 

C_DEPS += \
./custom/config/custom_sys_cfg.d \
./custom/config/sys_config.d 


# Each subdirectory must supply rules for building sources it contributes
custom/config/%.o: ../custom/config/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Windows GCC C Compiler (Sourcery Lite Bare)'
	arm-none-eabi-gcc -D__OCPU_COMPILER_GCC__ -D__CUSTOMER_CODE__ -I"${GCC_PATH}\arm-none-eabi\include" -I"C:\M66_OpenCPU_GS3_SDK_V2.0_Eclipse\include" -I"C:\M66_OpenCPU_GS3_SDK_V2.0_Eclipse\ril\inc" -I"C:\M66_OpenCPU_GS3_SDK_V2.0_Eclipse\custom\config" -I"C:\M66_OpenCPU_GS3_SDK_V2.0_Eclipse\custom\fota\inc" -O2 -Wall -std=c99 -c -fmessage-length=0 -mlong-calls -Wstrict-prototypes -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -march=armv5te -mthumb-interwork -mfloat-abi=soft -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


