#include <stdio.h>
#include "xparameters.h"
#include "xaxidma.h"
#include "xsysmon.h"
#include "xil_io.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "sleep.h"  // Add this for usleep()

#define DMA_DEV_ID XPAR_AXIDMA_0_DEVICE_ID  // AXI DMA Device ID
#define XADC_DEVICE_ID XPAR_SYSMON_0_DEVICE_ID  // XADC Device ID
#define DDR_BASE_ADDR 0x00100000  // Aligned DDR Address
#define TIMEOUT_MS 10  // Run for 0.1 seconds

XAxiDma AxiDma;  // AXI DMA instance
XSysMon SysMonInst;  // XADC instance

/**
 * @brief Initialize AXI DMA
 */
void init_DMA() {
    XAxiDma_Config *CfgPtr;
    int Status;

    CfgPtr = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (!CfgPtr) {
        xil_printf("ERROR: No DMA config found. Check address mapping.\n");
        return;
    }

    Status = XAxiDma_CfgInitialize(&AxiDma, CfgPtr);
    if (Status != XST_SUCCESS) {
        xil_printf("ERROR: DMA Initialization failed! Status = %d\n", Status);
        return;
    }

    XAxiDma_Reset(&AxiDma);
    while (!XAxiDma_ResetIsDone(&AxiDma));

    XAxiDma_WriteReg(DMA_DEV_ID, XAXIDMA_SR_OFFSET, 0xFFFFFFFF);  // Clear status
    xil_printf("DMA Initialized Successfully.\n");
}

/**
 * @brief Initialize XADC
 */
void init_XADC() {
    int Status;
    XSysMon_Config *ConfigPtr;

    ConfigPtr = XSysMon_LookupConfig(XADC_DEVICE_ID);
    if (!ConfigPtr) {
        xil_printf("ERROR: XADC config not found!\n");
        return;
    }

    Status = XSysMon_CfgInitialize(&SysMonInst, ConfigPtr, ConfigPtr->BaseAddress);
    if (Status != XST_SUCCESS) {
        xil_printf("ERROR: XADC Initialization failed!\n");
        return;
    }

    xil_printf("XADC Initialized Successfully.\n");
}

/**
 * @brief Capture ADC samples for 0.1 sec using usleep() and store to DDR via DMA
 */
void store_ADC_to_DMA() {
    u32 *adc_data = (u32 *)DDR_BASE_ADDR;
    int Status, timeout = 1000000;
    int sample_count = 0;

    xil_printf("\n===== Capturing ADC Samples for 0.1 sec =====\n");

    // **Capture ADC samples for 0.1 sec**
    usleep(100000);  // Sleep for 100ms

    for (int i = 0; i < 1000; i++) {  // Adjust based on how many samples fit in 0.1 sec
        adc_data[sample_count] = XSysMon_GetAdcData(&SysMonInst, XSM_CH_AUX_MIN + 1);
        xil_printf("Sample %04d: 0x%08X\n", sample_count, adc_data[sample_count]);
        sample_count++;
    }

    xil_printf("Captured %d samples in 0.1 sec\n", sample_count);

    // **Flush Cache BEFORE DMA Transfer**
    Xil_DCacheFlushRange(DDR_BASE_ADDR, sample_count * sizeof(u32));

    // **Ensure DMA Status is Cleared Before Transfer**
    XAxiDma_WriteReg(DMA_DEV_ID, XAXIDMA_SR_OFFSET, 0xFFFFFFFF);

    // **Start DMA Transfer**
    xil_printf("\nStarting ADC to DMA Transfer...\n");
    Status = XAxiDma_SimpleTransfer(&AxiDma, DDR_BASE_ADDR, sample_count * sizeof(u32), XAXIDMA_DEVICE_TO_DMA);

    if (Status != XST_SUCCESS) {
        xil_printf("ERROR: DMA Transfer failed! Status = %d\n", Status);
        return;
    }

    // **Check DMA Transfer Completion**
    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA) && timeout > 0) {
        timeout--;
        if (timeout % 100000 == 0) {
            xil_printf("DMA still busy...\n");
        }
    }

    if (timeout == 0) {
        xil_printf("ERROR: DMA Timeout! Status Register: 0x%08X\n", XAxiDma_ReadReg(DMA_DEV_ID, XAXIDMA_SR_OFFSET));
        return;
    }

    // **Invalidate Cache AFTER DMA Transfer**
    Xil_DCacheInvalidateRange(DDR_BASE_ADDR, sample_count * sizeof(u32));

    xil_printf("\n===== ADC Data Stored in DDR at 0x%X =====\n", DDR_BASE_ADDR);
}

/**
 * @brief Main Function
 */
int main() {
    xil_printf("\n===== ADC to DMA System Initialized =====\n");

    init_DMA();   // Initialize DMA
    init_XADC();  // Initialize XADC
    store_ADC_to_DMA();  // Capture ADC for 0.1s & transfer via DMA

    xil_printf("===== ADC Data Storage Completed =====\n");

    return 0;
}
