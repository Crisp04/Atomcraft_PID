// **Written by Shrinil Desai (25/02/2025) for Atomcraft**

#include <stdio.h>
#include "xparameters.h"
#include "xaxidma.h"
#include "xsysmon.h"
#include "xil_io.h"
#include "xil_cache.h"
#include "xtime_l.h"  // For precise timing

// **AXI DMA and XADC Definitions**
#define DMA_DEV_ID XPAR_AXIDMA_0_DEVICE_ID  // AXI DMA Device ID
#define XADC_DEVICE_ID XPAR_SYSMON_0_DEVICE_ID  // XADC Device ID
#define DDR_BASE_ADDR 0x00200000  // Aligned DDR Address
#define TIMEOUT_MS 100  // Run for 0.1 seconds


// **PWM Register Definitions**
#define PWM_BASE_ADDR XPAR_MY_PWM_CORE_0_S00_AXI_BASEADDR  // AXI PWM Base Address
#define PWM_REG0_OFFSET 0   // PWM0 Duty Cycle Register
#define PWM_REG1_OFFSET 4   // PWM1 Duty Cycle Register
#define PWM_COUNTER_MAX 254  // Maximum counter value
#define DUTY_CYCLE_75  (PWM_COUNTER_MAX * 3 / 4)  // 75% duty cycle
#define DUTY_CYCLE_25  (PWM_COUNTER_MAX * 1 / 4)  // 25% duty cycle

XAxiDma AxiDma;  // AXI DMA instance
XSysMon SysMonInst;  // XADC instance

/**
 * @brief Initialize PWM with 75% and 25% duty cycle
 */
void init_PWM() {
    Xil_Out32(PWM_BASE_ADDR + PWM_REG0_OFFSET, DUTY_CYCLE_75);
    Xil_Out32(PWM_BASE_ADDR + PWM_REG1_OFFSET, DUTY_CYCLE_25);
}

/**
 * @brief Initialize AXI DMA
 */
void init_DMA() {
    XAxiDma_Config *CfgPtr;
    int Status;

    CfgPtr = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (!CfgPtr) return;

    Status = XAxiDma_CfgInitialize(&AxiDma, CfgPtr);
    if (Status != XST_SUCCESS) return;

    XAxiDma_Reset(&AxiDma);
    while (!XAxiDma_ResetIsDone(&AxiDma));

    XAxiDma_WriteReg(DMA_DEV_ID, XAXIDMA_SR_OFFSET, 0xFFFFFFFF);  // Clear status
}

/**
 * @brief Initialize XADC
 */
void init_XADC() {
    XSysMon_Config *ConfigPtr;
    int Status;

    ConfigPtr = XSysMon_LookupConfig(XADC_DEVICE_ID);
    if (!ConfigPtr) return;

    Status = XSysMon_CfgInitialize(&SysMonInst, ConfigPtr, ConfigPtr->BaseAddress);
    if (Status != XST_SUCCESS) return;
}

/**
 * @brief Reset FIFO after each transfer
 */
void reset_DMA() {
    // Reset DMA
    XAxiDma_Reset(&AxiDma);
    while (!XAxiDma_ResetIsDone(&AxiDma));

}


/**
 * @brief Capture as many ADC samples as possible in 0.1 sec and store to DDR via DMA
 */
void store_ADC_to_DMA() {
    u32 *adc_data = (u32 *)DDR_BASE_ADDR;
    u16 adc12bit = 0;
    int Status, sample_count = 0;
    int current_mA = 0;
    XTime start_time, current_time;

    XTime_GetTime(&start_time);

    while (1) {
        XTime_GetTime(&current_time);

        // Stop sampling if 0.1 seconds has passed
        if ((current_time - start_time) >= 10000000) {
            break;
        }

        // Get raw 16-bit value from XADC
        u16 rawAdc = XSysMon_GetAdcData(&SysMonInst, XSM_CH_AUX_MIN);

        // Convert to 12-bit ADC value
        adc12bit = rawAdc >> 4;
       //printf("12-bit ADC Value: %d\n", adc12bit);


        // Convert to voltage (ACS712-05B outputs between 0 - 3.3v)
        float voltage = (adc12bit / 4095.0) * 3.3;
        //printf("Sensor Voltage: %.3f V\n", voltage);


        // Convert to current (ACS712-05B: 185 mV/A and 1.65V midpoint)
        float current = (voltage - 1.65) / 0.185;
        //printf("Current: %d mA\r\n", current_mA);


        // Optional: convert to milliamps
        current_mA = (int)(current * 1000.0);

        // Store as 32-bit integer
        adc_data[sample_count++] = (u32)current_mA;
    }

    xil_printf("Count : %d\n", sample_count);



    // **Reset DMA after transfer**
    reset_DMA();
}


/**
 * @brief Main Function
 */
int main() {
    xil_printf("===== Start =====\n");
    init_PWM();
    init_DMA();
    init_XADC();
    store_ADC_to_DMA();
    xil_printf("===== Stop =====\n");

    return 0;
}
