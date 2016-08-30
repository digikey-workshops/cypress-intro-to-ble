/*****************************************************************************
* File Name: main.c
*
* Version: 1.0
*
* Description:
* This is the top level application for the PSoC 4 BLE Lab 4.
*
* Hardware Dependency:
* CY8CKIT-042 BLE Pioneer Kit
*
******************************************************************************
* Copyright (2014), Cypress Semiconductor Corporation.
******************************************************************************
* This software is owned by Cypress Semiconductor Corporation (Cypress) and is
* protected by and subject to worldwide patent protection (United States and
* foreign), United States copyright laws and international treaty provisions.
* Cypress hereby grants to licensee a personal, non-exclusive, non-transferable
* license to copy, use, modify, create derivative works of, and compile the
* Cypress Source Code and derivative works for the sole purpose of creating
* custom software in support of licensee product to be used only in conjunction
* with a Cypress integrated circuit as specified in the applicable agreement.
* Any reproduction, modification, translation, compilation, or representation of
* this software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: CYPRESS MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH
* REGARD TO THIS MATERIAL, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* Cypress reserves the right to make changes without further notice to the
* materials described herein. Cypress does not assume any liability arising out
* of the application or use of any product or circuit described herein. Cypress
* does not authorize its products for use as critical components in life-support
* systems where a malfunction or failure may reasonably be expected to result in
* significant injury to the user. The inclusion of Cypress' product in a life-
* support systems application implies that the manufacturer assumes all risk of
* such use and in doing so indemnifies Cypress against all charges. Use may be
* limited by and subject to the applicable Cypress software license agreement.
*****************************************************************************/


/*****************************************************************************
* Included headers
*****************************************************************************/
#include <main.h>
#include <BLEApplications.h>


/*****************************************************************************
* Function Prototypes
*****************************************************************************/
static void InitializeSystem(void);
static void HandleCapSenseSlider(void);


/*****************************************************************************
* Public functions
*****************************************************************************/

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* System entrance point. This calls the initializing function and continuously
* process BLE and CapSense events.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main()
{
    CYBLE_LP_MODE_T bleMode;
    uint8 interruptStatus;

    /* This function will initialize the system resources such as BLE and CapSense */
    InitializeSystem();
	
    for(;;)
    {
        if(TRUE == deviceConnected)
		{
            /* Send CapSense Slider data when respective notification is enabled */
			if(TRUE == sendCapSenseSliderNotifications)
			{
				/* Check for CapSense slider swipe and send data accordingly */
				HandleCapSenseSlider();
			}
		}
        
        /*Process event callback to handle BLE events. The events generated and 
		* used for this application are inside the 'CustomEventHandler' routine*/
        CyBle_ProcessEvents();
		
        /* The idea of low power operation is to first request the BLE 
         * block go to Deep Sleep, and then check whether it actually
         * entered Deep Sleep. This is important because the BLE block
         * runs asynchronous to the rest of the application and thus
         * could be busy/idle independent of the application state. 
         * 
         * Once the BLE block is in Deep Sleep, only then the system 
         * can enter Deep Sleep. This is important to maintain the BLE 
         * connection alive while being in Deep Sleep.
         */

            
        /* Request the BLE block to enter Deep Sleep */
        bleMode = CyBle_EnterLPM(CYBLE_BLESS_DEEPSLEEP);

        
        /* Check if the BLE block entered Deep Sleep and if so, then the 
         * system can enter Deep Sleep. This is done inside a Critical 
         * Section (where global interrupts are disabled) to avoid a 
         * race condition between application main (that wants to go to 
         * Deep Sleep) and other interrupts (which keep the device from 
         * going to Deep Sleep). 
         */
        interruptStatus = CyEnterCriticalSection();
        
        /* Check if the BLE block entered Deep Sleep */
        if(CYBLE_BLESS_DEEPSLEEP == bleMode)
        {
            /* Check the current state of BLE - System can enter Deep Sleep
             * only when the BLE block is starting the ECO (during 
             * pre-processing for a new connection event) or when it is 
             * idle.
             */
            if((CyBle_GetBleSsState() == CYBLE_BLESS_STATE_ECO_ON) ||
               (CyBle_GetBleSsState() == CYBLE_BLESS_STATE_DEEPSLEEP))
            {
                /* PrISM components are not functional in system Deep Sleep
                 * mode. Enabling Deep Sleep will kill the RGB LED functionality
                 * in this project. */
                /* Refer to PSoC_4_CapSense_Slider_LED example project (installed 
                 * with CY8CKIT-042-BLE) for Deep-Sleep operation with the PrISM 
                 * Component */
                CySysPmSleep(); 
            }
        }
        /* The else condition signifies that the BLE block cannot enter 
         * Deep Sleep and is in Active mode.  
         */
        else
        {
            /* At this point, the CPU can enter Sleep, but Deep Sleep is not
             * allowed. 
             * There is one exception - at a connection event, when the BLE 
             * Rx/Tx has just finished, and the post processing for the 
             * connection event is ongoing, the CPU cannot go to sleep.
             * The CPU should wait in Active mode until the post processing 
             * is complete while continuously polling the BLE low power 
             * entry. As soon as post processing is complete, the BLE block 
             * would enter Deep Sleep (because of the polling) and the 
             * system Deep Sleep would then be entered. Deep Sleep is the 
             * preferred low power mode since it takes much lesser current.
             */
            if(CyBle_GetBleSsState() != CYBLE_BLESS_STATE_EVENT_CLOSE)
            {
                CySysPmSleep();
            }
        }
        
        /* Exit Critical section - Global interrupts are enabled again */
        CyExitCriticalSection(interruptStatus);
        

        /* Hibernate entry point - Hibernate is entered upon a BLE disconnect
         * event or advertisement timeout. Wakeup happens via SW2 switch press, 
         * upon which the execution starts from the first line of code. 
         * The I/O state, RAM and UDBs are retained during Hibernate.
         */
        if(enterHibernateFlag)
        {
            /* Stop the BLE component */
            CyBle_Stop();
            
            /* Enable the Hibernate wakeup functionality */
            SW2_Switch_ClearInterrupt();
            Wakeup_ISR_Start();
            
            /* The RGB LED on BLE Pioneer kit are active low. Drive HIGH on 
             * pin for OFF and drive LOW on pin for ON*/
            PRS_1_WritePulse0(RGB_LED_OFF);
            PRS_1_WritePulse1(RGB_LED_OFF);
            PRS_2_WritePulse0(RGB_LED_OFF);

            /* Set Drive mode of output pins from HiZ to Strong */
            RED_SetDriveMode(RED_DM_ALG_HIZ);
            GREEN_SetDriveMode(GREEN_DM_ALG_HIZ);
            BLUE_SetDriveMode(BLUE_DM_ALG_HIZ);
            
            #if (RGB_LED_IN_PROJECT)
                /* Turn off Green and Blue LEDs to indicate Hibernate */
                Led_Advertising_Green_Write(1);
                Led_Connected_Blue_Write(1);
            #endif  /* #if (RGB_LED_IN_PROJECT) */
            
            /* Enter hibernate mode */
            CySysPmHibernate();
        }
    }	
}


/*******************************************************************************
* Function Name: InitializeSystem
********************************************************************************
* Summary:
* Start the components and initialize system.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void InitializeSystem(void)
{
	/* Enable global interrupt mask */
	CyGlobalIntEnable; 
		
	/* Start BLE component and register the CustomEventHandler function. This 
	 * function exposes the events from BLE component for application use */
    CyBle_Start(CustomEventHandler);	
    
	/* Start both the PrISM components for LED control*/
    PRS_1_Start();
    PRS_2_Start();
	
	/* The RGB LED on BLE Pioneer kit are active low. Drive HIGH on 
	 * pin for OFF and drive LOW on pin for ON*/
	PRS_1_WritePulse0(RGB_LED_OFF);
	PRS_1_WritePulse1(RGB_LED_OFF);
	PRS_2_WritePulse0(RGB_LED_OFF);
	
	/* Set Drive mode of output pins from HiZ to Strong */
	RED_SetDriveMode(RED_DM_STRONG);
	GREEN_SetDriveMode(GREEN_DM_STRONG);
	BLUE_SetDriveMode(BLUE_DM_STRONG);
	
	/* ADD_CODE to initialize CapSense component and initialize baselines*/
	CapSense_Start();
	CapSense_InitializeAllBaselines();
}


/*******************************************************************************
* Function Name: HandleCapSenseSlider
********************************************************************************
* Summary:
* This function scans for finger position on CapSense slider, and if the  
* position is different, triggers separate routine for BLE notification.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void HandleCapSenseSlider(void)
{
	/* Last read CapSense slider position value */
	static uint16 lastPosition;	
	
	/* Present slider position read by CapSense */
	uint16 sliderPosition;
		
	/* Update CapSense baseline for next reading*/
	CapSense_UpdateEnabledBaselines();	
		
	/* ADD_CODE to scan the slider widget */
	CapSense_ScanEnabledWidgets();			
	
	/* Wait for CapSense scanning to be complete. This could take about 5 ms */
	while(CapSense_IsBusy());
	
	/* ADD_CODE to read the finger position on the slider */
	sliderPosition = CapSense_GetCentroidPos(CapSense_LINEARSLIDER0__LS);	

	/* If finger position on the slider is changed then send data as BLE notifications */
	if(sliderPosition != lastPosition)
	{
		/*If finger is detected on the slider*/
		if((sliderPosition == NO_FINGER) || (sliderPosition <= SLIDER_MAX_VALUE))
		{
			/* Send data over Slider Notification */
			SendCapSenseNotification((uint8)sliderPosition);

		}	/* if(sliderPosition != NO_FINGER) */
		
		/* Update local static variable with present finger position on slider*/
		lastPosition = sliderPosition;
		
	}	/* if(sliderPosition != lastPosition) */	
}

/* [] END OF FILE */
