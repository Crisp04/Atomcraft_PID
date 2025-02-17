// Written by Shrinil Desai for Atomcraft 2024

#include "xparameters.h"
#include "xil_io.h"
#include "xadcps.h"    // Include XADC header for Zynq XADC functionality
#include "xil_printf.h"

// Define base address of the PWM core
#define PWM_BASE_ADDR  XPAR_MY_PWM_CORE_SHIFTED_0_S00_AXI_BASEADDR  // Replace with actual base address if necessary

// Define register offsets
#define PWM_REG0_OFFSET 0   // Offset for slv_reg0
#define PWM_REG1_OFFSET 4   // Offset for slv_reg1

// Define PWM settings
#define PWM_COUNTER_MAX 1024   // Maximum counter value
#define DUTY_CYCLE_75  (PWM_COUNTER_MAX * (3 / 4))  // 75% duty cycle
#define DUTY_CYCLE_25 (PWM_COUNTER_MAX / 2)

// Define XADC device ID
#define XADC_DEVICE_ID XPAR_XADCPS_0_DEVICE_ID

// Function to initialize the XADC
int initXADC(XAdcPs *XAdcInstPtr) {
    XAdcPs_Config *ConfigPtr;
    int Status;

    // Initialize the XADC driver
    ConfigPtr = XAdcPs_LookupConfig(XADC_DEVICE_ID);
    if (ConfigPtr == NULL) {
        xil_printf("XADC LookupConfig failed.\n");
        return XST_FAILURE;
    }

    // Initialize the XADC instance
    Status = XAdcPs_CfgInitialize(XAdcInstPtr, ConfigPtr, ConfigPtr->BaseAddress);
    if (Status != XST_SUCCESS) {
        xil_printf("XADC Initialization failed.\n");
        return XST_FAILURE;
    }

    // Set the XADC to continuous mode and disable averaging
    XAdcPs_SetSequencerMode(XAdcInstPtr, XADCPS_SEQ_MODE_CONTINPASS);
    XAdcPs_SetAvg(XAdcInstPtr, XADCPS_AVG_16_SAMPLES);  // Use 16 sample averaging


    return XST_SUCCESS;
}

int main() {
    XAdcPs XAdcInst;  // XADC instance
    u16 RawData_1;
    u16 RawData_2;
    u16 RawData_Vpvn;
    u16 RawData_Vaux0;
    int VoltageMillivolts;  // Store voltage in millivolts
    int TemperatureCelsius;  // Store temperature in Celsius
    int Duty_Cycle1; // Store duty cycle from 3.3v in
    int Duty_Cycle2; // Store duty cycle from 1v in (Vp_Vn)

    // Initialize the XADC
    if (initXADC(&XAdcInst) != XST_SUCCESS) {
        xil_printf("Failed to initialize XADC.\n");
        return XST_FAILURE;
    }

    // Set the duty cycle to 75% for the normal PWM signal (PWM0)
    Xil_Out32(PWM_BASE_ADDR + PWM_REG0_OFFSET, DUTY_CYCLE_75);

    // Set the duty cycle to 75% for the inverted, phase-shifted PWM signal (PWM1)
    Xil_Out32(PWM_BASE_ADDR + PWM_REG1_OFFSET, DUTY_CYCLE_75);

    xil_printf("PWM and XADC initialized.\n");
    xil_printf("RawData_1,RawData_2,RawData_Vpvn,RawData_Vaux0,VoltageMillivolts,TemperatureCelsius,Duty_Cycle1,Duty_Cycle2\n");

    // Infinite loop to keep reading the XADC and printing the results
    while (1) {
        Xil_Out32(PWM_BASE_ADDR + PWM_REG0_OFFSET, DUTY_CYCLE_75);

        // Set the duty cycle to 75% for the inverted, phase-shifted PWM signal (PWM1)
        Xil_Out32(PWM_BASE_ADDR + PWM_REG1_OFFSET, DUTY_CYCLE_75);

    	// Read the XADC data from the VCCINT channel
        RawData_1 = XAdcPs_GetAdcData(&XAdcInst, XADCPS_CH_VCCINT);
        RawData_2 = XAdcPs_GetAdcData(&XAdcInst, XADCPS_CH_TEMP);
        RawData_Vpvn = XAdcPs_GetAdcData(&XAdcInst, XADCPS_CH_VPVN);
        RawData_Vaux0 = XAdcPs_GetAdcData(&XAdcInst, XADCPS_CH_AUX_MIN + 1);


        // Debug: Print the raw ADC data
        xil_printf("Raw VCCINT XADC Data: %d\n", RawData_1);
        xil_printf("Raw TEMP XADC Data: %d\n", RawData_2);
        xil_printf("Raw VP_VN XADC Data: %d\n", RawData_Vpvn);
        xil_printf("Raw Input XADC Data: %d\n", RawData_Vaux0);

        xil_printf("\n");

        // Convert raw data to millivolts (0-1000mV range for 1.0V system)
        VoltageMillivolts = ((int)((float)RawData_1 / 65536.0 * 1000.0));  // Scale to millivolts for 1.0V system

        // Print the VCCINT voltage in millivolts
        xil_printf("VCCINT Voltage: %d mV\n", VoltageMillivolts);


        // Convert raw data to temperature in Celsius
        TemperatureCelsius = ((float)RawData_2 / 65536.0) * 503.975 - 273.15;

        // Conver raw data to duty cycle
        Duty_Cycle1 = ((float)RawData_Vaux0 / 65535.0) * 100;
        Duty_Cycle2 = ((float)RawData_Vpvn / 65535.0) * 100;
        xil_printf("%d,%d,%d,%d,%d,%d,%d,%d\n", RawData_1, RawData_2, RawData_Vpvn, RawData_Vaux0, VoltageMillivolts, TemperatureCelsius, Duty_Cycle1, Duty_Cycle2);


        // Print the temperature in degrees Celsius
       // xil_printf("FPGA Temperature: %d C\n", TemperatureCelsius);
        //xil_printf("XADC input duty cycle (3.3v): %d Percent\n", Duty_Cycle1);
        //xil_printf("XADC input duty cycle (1v): %d Percent\n", Duty_Cycle2);


        xil_printf("\n");
        for (volatile int i = 0; i < 100000000; i++);
        // Set the duty cycle to 75% for the normal PWM signal (PWM0)
        Xil_Out32(PWM_BASE_ADDR + PWM_REG0_OFFSET, DUTY_CYCLE_25);

        // Set the duty cycle to 75% for the inverted, phase-shifted PWM signal (PWM1)
        Xil_Out32(PWM_BASE_ADDR + PWM_REG1_OFFSET, DUTY_CYCLE_25);

        xil_printf("Raw VCCINT XADC Data: %d\n", RawData_1);
        xil_printf("Raw TEMP XADC Data: %d\n", RawData_2);
        xil_printf("Raw VP_VN XADC Data: %d\n", RawData_Vpvn);
        xil_printf("Raw Input XADC Data: %d\n", RawData_Vaux0);
        for (volatile int i = 0; i < 100000000; i++);


        xil_printf("Raw VCCINT XADC Data: %d\n", RawData_1);
        xil_printf("Raw TEMP XADC Data: %d\n", RawData_2);
        xil_printf("Raw VP_VN XADC Data: %d\n", RawData_Vpvn);
        xil_printf("Raw Input XADC Data: %d\n", RawData_Vaux0);



        // Add a delay (optional) to avoid printing too frequently
        for (volatile int i = 0; i < 100000000; i++);

    }



    return 0;
}
