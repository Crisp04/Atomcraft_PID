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
#define DDR_BASE_ADDR 0x00100000  // Aligned DDR Address
#define TIMEOUT_MS 100  // Run for 0.1 seconds

// **PWM Register Definitions**
#define PWM_BASE_ADDR XPAR_MY_PWM_CORE_0_S00_AXI_BASEADDR  // AXI PWM Base Address
#define PWM_REG0_OFFSET 0   // PWM0 Duty Cycle Register
#define PWM_REG1_OFFSET 4   // PWM1 Duty Cycle Register
#define PWM_COUNTER_MAX 1024  // Maximum counter value
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
 * @brief Capture as many ADC samples as possible in 0.1 sec and store to DDR via DMA
 */
void store_ADC_to_DMA() {
    u32 *adc_data = (u32 *)DDR_BASE_ADDR;
    int Status, sample_count = 0;
    XTime start_time, current_time;

    XTime_GetTime(&start_time);

    // **Capture ADC samples until 0.1 sec elapses**
    do {
        // Capture a batch of samples
        for (int i = 0; i < 1024; i++) {
            adc_data[sample_count++] = XSysMon_GetAdcData(&SysMonInst, XSM_CH_AUX_MIN);
        }

        // Check elapsed time only after capturing a batch
        XTime_GetTime(&current_time);

    } while (((current_time - start_time) * 1000.0 / COUNTS_PER_SECOND) < TIMEOUT_MS);

    // **Flush Cache BEFORE DMA Transfer**
    Xil_DCacheFlushRange(DDR_BASE_ADDR, sample_count * sizeof(u32));

    // **Ensure DMA Status is Cleared Before Transfer**
    XAxiDma_WriteReg(DMA_DEV_ID, XAXIDMA_SR_OFFSET, 0xFFFFFFFF);

    // **Start DMA Transfer**
    Status = XAxiDma_SimpleTransfer(&AxiDma, DDR_BASE_ADDR, sample_count * sizeof(u32), XAXIDMA_DEVICE_TO_DMA);

    // **Invalidate Cache AFTER DMA Transfer**
    Xil_DCacheInvalidateRange(DDR_BASE_ADDR, sample_count * sizeof(u32));
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
    xil_printf("===== Stop =====");

    return 0;
}
