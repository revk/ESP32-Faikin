/* Automatically generated file; DO NOT EDIT */
/* Espressif IoT Development Framework Linker Script */
/* Generated from: /home/adrian/ESP32/5.0.2/components/esp_system/ld/esp32/sections.ld.in */

/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*  Default entry point:  */
ENTRY(call_start_cpu0);

SECTIONS
{
  /* RTC fast memory holds RTC wake stub code,
     including from any source file named rtc_wake_stub*.c
  */
  .rtc.text :
  {
    . = ALIGN(4);

    *(.rtc.literal .rtc.text .rtc.text.*)

    *rtc_wake_stub*.*(.literal .text .literal.* .text.*)
    _rtc_text_end = ABSOLUTE(.);
  } > rtc_iram_seg

  /*
    This section is required to skip rtc.text area because rtc_iram_seg and
    rtc_data_seg are reflect the same address space on different buses.
  */
  .rtc.dummy :
  {
    _rtc_dummy_start = ABSOLUTE(.);
    _rtc_fast_start = ABSOLUTE(.);
    . = SIZEOF(.rtc.text);
    _rtc_dummy_end = ABSOLUTE(.);
  } > rtc_data_seg

  /* This section located in RTC FAST Memory area.
     It holds data marked with RTC_FAST_ATTR attribute.
     See the file "esp_attr.h" for more information.
  */
  .rtc.force_fast :
  {
    . = ALIGN(4);
    _rtc_force_fast_start = ABSOLUTE(.);

    _coredump_rtc_fast_start = ABSOLUTE(.);
    *(.rtc.fast.coredump .rtc.fast.coredump.*)
    _coredump_rtc_fast_end = ABSOLUTE(.);

    *(.rtc.force_fast .rtc.force_fast.*)
    . = ALIGN(4) ;

    _rtc_force_fast_end = ABSOLUTE(.);
  } > rtc_data_seg

  /* RTC data section holds RTC wake stub
     data/rodata, including from any source file
     named rtc_wake_stub*.c and the data marked with
     RTC_DATA_ATTR, RTC_RODATA_ATTR attributes.
     The memory location of the data is dependent on
     CONFIG_ESP32_RTCDATA_IN_FAST_MEM option.
  */
  .rtc.data :
  {
    _rtc_data_start = ABSOLUTE(.);

    _coredump_rtc_start = ABSOLUTE(.);
    *(.rtc.coredump .rtc.coredump.*)
    _coredump_rtc_end = ABSOLUTE(.);
    *(.rtc.data .rtc.data.*)
    *(.rtc.rodata .rtc.rodata.*)

    *rtc_wake_stub*.*(.data .rodata .data.* .rodata.* .bss .bss.*)
    _rtc_data_end = ABSOLUTE(.);

  } > rtc_data_location

  /* RTC bss, from any source file named rtc_wake_stub*.c */
  .rtc.bss (NOLOAD) :
  {
    _rtc_bss_start = ABSOLUTE(.);
    *rtc_wake_stub*.*(.bss .bss.*)
    *rtc_wake_stub*.*(COMMON)

    *(.rtc.bss)

    _rtc_bss_end = ABSOLUTE(.);
  } > rtc_data_location

  /* This section holds data that should not be initialized at power up
     and will be retained during deep sleep.
     User data marked with RTC_NOINIT_ATTR will be placed
     into this section. See the file "esp_attr.h" for more information.
	 The memory location of the data is dependent on
     CONFIG_ESP32_RTCDATA_IN_FAST_MEM option.
  */
  .rtc_noinit (NOLOAD):
  {
    . = ALIGN(4);
    _rtc_noinit_start = ABSOLUTE(.);
    *(.rtc_noinit .rtc_noinit.*)
    . = ALIGN(4) ;
    _rtc_noinit_end = ABSOLUTE(.);
  } > rtc_data_location

  /* This section located in RTC SLOW Memory area.
     It holds data marked with RTC_SLOW_ATTR attribute.
     See the file "esp_attr.h" for more information.
  */
  .rtc.force_slow :
  {
    . = ALIGN(4);
    _rtc_force_slow_start = ABSOLUTE(.);
    *(.rtc.force_slow .rtc.force_slow.*)
    . = ALIGN(4) ;
    _rtc_force_slow_end = ABSOLUTE(.);
  } > rtc_slow_seg

  /* Get size of rtc slow data based on rtc_data_location alias */
  _rtc_slow_length = (ORIGIN(rtc_slow_seg) == ORIGIN(rtc_data_location))
                        ? (_rtc_force_slow_end - _rtc_data_start)
                        : (_rtc_force_slow_end - _rtc_force_slow_start);

  _rtc_fast_length = (ORIGIN(rtc_slow_seg) == ORIGIN(rtc_data_location))
                        ? (_rtc_force_fast_end - _rtc_fast_start)
                        : (_rtc_noinit_end - _rtc_fast_start);

  ASSERT((_rtc_slow_length <= LENGTH(rtc_slow_seg)),
          "RTC_SLOW segment data does not fit.")

  ASSERT((_rtc_fast_length <= LENGTH(rtc_data_seg)),
          "RTC_FAST segment data does not fit.")

  /* Send .iram0 code to iram */
  .iram0.vectors :
  {
    _iram_start = ABSOLUTE(.);
    /* Vectors go to IRAM */
    _vector_table = ABSOLUTE(.);
    /* Vectors according to builds/RF-2015.2-win32/esp108_v1_2_s5_512int_2/config.html */
    . = 0x0;
    KEEP(*(.WindowVectors.text));
    . = 0x180;
    KEEP(*(.Level2InterruptVector.text));
    . = 0x1c0;
    KEEP(*(.Level3InterruptVector.text));
    . = 0x200;
    KEEP(*(.Level4InterruptVector.text));
    . = 0x240;
    KEEP(*(.Level5InterruptVector.text));
    . = 0x280;
    KEEP(*(.DebugExceptionVector.text));
    . = 0x2c0;
    KEEP(*(.NMIExceptionVector.text));
    . = 0x300;
    KEEP(*(.KernelExceptionVector.text));
    . = 0x340;
    KEEP(*(.UserExceptionVector.text));
    . = 0x3C0;
    KEEP(*(.DoubleExceptionVector.text));
    . = 0x400;
    _invalid_pc_placeholder = ABSOLUTE(.);
    *(.*Vector.literal)

    *(.UserEnter.literal);
    *(.UserEnter.text);
    . = ALIGN (16);
    *(.entry.text)
    *(.init.literal)
    *(.init)

    _init_end = ABSOLUTE(.);
  } > iram0_0_seg

  .iram0.text :
  {
    /* Code marked as runnning out of IRAM */
    _iram_text_start = ABSOLUTE(.);

    *(.iram1 .iram1.*)
    *libapp_trace.a:app_trace.*(.literal .literal.* .text .text.*)
    *libapp_trace.a:app_trace_util.*(.literal .literal.* .text .text.*)
    *libapp_trace.a:port_uart.*(.literal .literal.* .text .text.*)
    *libcoexist.a:(.wifislpiram .wifislpiram.*)
    *libesp_event.a:default_event_loop.*(.literal.esp_event_isr_post .text.esp_event_isr_post)
    *libesp_event.a:esp_event.*(.literal.esp_event_isr_post_to .text.esp_event_isr_post_to)
    *libesp_hw_support.a:cpu.*(.literal.esp_cpu_compare_and_set .text.esp_cpu_compare_and_set)
    *libesp_hw_support.a:cpu.*(.literal.esp_cpu_reset .text.esp_cpu_reset)
    *libesp_hw_support.a:cpu.*(.literal.esp_cpu_stall .text.esp_cpu_stall)
    *libesp_hw_support.a:cpu.*(.literal.esp_cpu_unstall .text.esp_cpu_unstall)
    *libesp_hw_support.a:cpu.*(.literal.esp_cpu_wait_for_intr .text.esp_cpu_wait_for_intr)
    *libesp_hw_support.a:esp_memory_utils.*(.literal .literal.* .text .text.*)
    *libesp_hw_support.a:periph_ctrl.*(.literal.periph_module_reset .text.periph_module_reset)
    *libesp_hw_support.a:periph_ctrl.*(.literal.wifi_module_disable .text.wifi_module_disable)
    *libesp_hw_support.a:periph_ctrl.*(.literal.wifi_module_enable .text.wifi_module_enable)
    *libesp_hw_support.a:rtc_clk.*(.literal .literal.* .text .text.*)
    *libesp_hw_support.a:rtc_init.*(.literal.rtc_vddsdio_set_config .text.rtc_vddsdio_set_config)
    *libesp_hw_support.a:rtc_pm.*(.literal .literal.* .text .text.*)
    *libesp_hw_support.a:rtc_sleep.*(.literal .literal.* .text .text.*)
    *libesp_hw_support.a:rtc_time.*(.literal .literal.* .text .text.*)
    *libesp_hw_support.a:rtc_wdt.*(.literal .literal.* .text .text.*)
    *libesp_phy.a:phy_init.*(.literal.esp_phy_disable .text.esp_phy_disable)
    *libesp_phy.a:phy_init.*(.literal.esp_phy_enable .text.esp_phy_enable)
    *libesp_phy.a:phy_init.*(.literal.esp_wifi_bt_power_domain_off .text.esp_wifi_bt_power_domain_off)
    *libesp_psram.a:mmu.*(.literal .literal.* .text .text.*)
    *libesp_ringbuf.a:(EXCLUDE_FILE(*libesp_ringbuf.a:ringbuf.*) .literal EXCLUDE_FILE(*libesp_ringbuf.a:ringbuf.*) .literal.* EXCLUDE_FILE(*libesp_ringbuf.a:ringbuf.*) .text EXCLUDE_FILE(*libesp_ringbuf.a:ringbuf.*) .text.*)
    *libesp_ringbuf.a:ringbuf.*(.literal.prvAcquireItemNoSplit .literal.prvCheckItemFitsByteBuffer .literal.prvCheckItemFitsDefault .literal.prvCopyItemAllowSplit .literal.prvCopyItemByteBuf .literal.prvCopyItemNoSplit .literal.prvGetFreeSize .literal.prvGetItemByteBuf .literal.prvGetItemDefault .literal.prvReceiveGenericFromISR .literal.prvReturnItemByteBuf .literal.prvReturnItemDefault .literal.prvSendItemDoneNoSplit .literal.vRingbufferReturnItemFromISR .literal.xRingbufferCreateNoSplit .literal.xRingbufferGetCurFreeSize .literal.xRingbufferGetMaxItemSize .literal.xRingbufferPrintInfo .literal.xRingbufferReceiveFromISR .literal.xRingbufferReceiveSplitFromISR .literal.xRingbufferReceiveUpToFromISR .literal.xRingbufferSendAcquire .literal.xRingbufferSendComplete .literal.xRingbufferSendFromISR .text .text.prvAcquireItemNoSplit .text.prvCheckItemAvail .text.prvCheckItemFitsByteBuffer .text.prvCheckItemFitsDefault .text.prvCopyItemAllowSplit .text.prvCopyItemByteBuf .text.prvCopyItemNoSplit .text.prvGetFreeSize .text.prvGetItemByteBuf .text.prvGetItemDefault .text.prvReceiveGenericFromISR .text.prvReturnItemByteBuf .text.prvReturnItemDefault .text.prvSendItemDoneNoSplit .text.vRingbufferReturnItemFromISR .text.xRingbufferCreateNoSplit .text.xRingbufferGetCurFreeSize .text.xRingbufferGetMaxItemSize .text.xRingbufferPrintInfo .text.xRingbufferReceiveFromISR .text.xRingbufferReceiveSplitFromISR .text.xRingbufferReceiveUpToFromISR .text.xRingbufferSendAcquire .text.xRingbufferSendComplete .text.xRingbufferSendFromISR)
    *libesp_rom.a:esp_rom_regi2c.*(.literal .literal.* .text .text.*)
    *libesp_rom.a:esp_rom_spiflash.*(.literal .literal.* .text .text.*)
    *libesp_system.a:esp_err.*(.literal .literal.* .text .text.*)
    *libesp_system.a:esp_system.*(.literal.esp_system_abort .text.esp_system_abort)
    *libesp_system.a:ubsan.*(.literal .literal.* .text .text.*)
    *libesp_timer.a:esp_timer.*(.literal.timer_process_alarm .text.timer_process_alarm)
    *libesp_timer.a:esp_timer.*(.literal.timer_task .text.timer_task)
    *libesp_wifi.a:esp_adapter.*(.literal.wifi_clock_disable_wrapper .text.wifi_clock_disable_wrapper)
    *libesp_wifi.a:esp_adapter.*(.literal.wifi_clock_enable_wrapper .text.wifi_clock_enable_wrapper)
    *libfreertos.a:(EXCLUDE_FILE(*libfreertos.a:event_groups.* *libfreertos.a:port.* *libfreertos.a:port_common.* *libfreertos.a:queue.* *libfreertos.a:tasks.* *libfreertos.a:timers.*) .literal EXCLUDE_FILE(*libfreertos.a:event_groups.* *libfreertos.a:port.* *libfreertos.a:port_common.* *libfreertos.a:queue.* *libfreertos.a:tasks.* *libfreertos.a:timers.*) .literal.* EXCLUDE_FILE(*libfreertos.a:event_groups.* *libfreertos.a:port.* *libfreertos.a:port_common.* *libfreertos.a:queue.* *libfreertos.a:tasks.* *libfreertos.a:timers.*) .text EXCLUDE_FILE(*libfreertos.a:event_groups.* *libfreertos.a:port.* *libfreertos.a:port_common.* *libfreertos.a:queue.* *libfreertos.a:tasks.* *libfreertos.a:timers.*) .text.*)
    *libfreertos.a:event_groups.*(.literal.vEventGroupClearBitsCallback .literal.vEventGroupSetBitsCallback .literal.xEventGroupGetBitsFromISR .text .text.vEventGroupClearBitsCallback .text.vEventGroupSetBitsCallback .text.xEventGroupGetBitsFromISR)
    *libfreertos.a:port.*(.literal.vApplicationStackOverflowHook .literal.vPortAssertIfInISR .literal.vPortEndScheduler .literal.vPortExitCritical .literal.vPortExitCriticalCompliance .literal.vPortSetStackWatchpoint .literal.vPortTaskWrapper .literal.vPortYieldOtherCore .literal.xPortEnterCriticalTimeout .literal.xPortEnterCriticalTimeoutCompliance .literal.xPortInIsrContext .text .text.vApplicationStackOverflowHook .text.vPortAssertIfInISR .text.vPortEndScheduler .text.vPortExitCritical .text.vPortExitCriticalCompliance .text.vPortSetStackWatchpoint .text.vPortTaskWrapper .text.vPortYieldOtherCore .text.xPortEnterCriticalTimeout .text.xPortEnterCriticalTimeoutCompliance .text.xPortGetTickRateHz .text.xPortInIsrContext)
    *libfreertos.a:port_common.*(.literal.esp_startup_start_app_common .literal.other_cpu_startup_idle_hook_cb .literal.vApplicationGetIdleTaskMemory .literal.vApplicationGetTimerTaskMemory .literal.xPortCheckValidTCBMem .literal.xPortcheckValidStackMem .text .text.esp_startup_start_app_common .text.other_cpu_startup_idle_hook_cb .text.vApplicationGetIdleTaskMemory .text.vApplicationGetTimerTaskMemory .text.xPortCheckValidTCBMem .text.xPortcheckValidStackMem)
    *libfreertos.a:queue.*(.literal.prvCopyDataFromQueue .literal.prvCopyDataToQueue .literal.prvIsQueueEmpty .literal.prvNotifyQueueSetContainer .literal.prvUnlockQueue .literal.uxQueueMessagesWaitingFromISR .literal.xQueueGenericSendFromISR .literal.xQueueGetMutexHolderFromISR .literal.xQueueGiveFromISR .literal.xQueueIsQueueEmptyFromISR .literal.xQueueIsQueueFullFromISR .literal.xQueuePeek .literal.xQueuePeekFromISR .literal.xQueueReceive .literal.xQueueReceiveFromISR .literal.xQueueSelectFromSetFromISR .literal.xQueueSemaphoreTake .text .text.prvCopyDataFromQueue .text.prvCopyDataToQueue .text.prvGetDisinheritPriorityAfterTimeout .text.prvIsQueueEmpty .text.prvNotifyQueueSetContainer .text.prvUnlockQueue .text.uxQueueMessagesWaitingFromISR .text.xQueueGenericSendFromISR .text.xQueueGetMutexHolderFromISR .text.xQueueGiveFromISR .text.xQueueIsQueueEmptyFromISR .text.xQueueIsQueueFullFromISR .text.xQueuePeek .text.xQueuePeekFromISR .text.xQueueReceive .text.xQueueReceiveFromISR .text.xQueueSelectFromSetFromISR .text.xQueueSemaphoreTake)
    *libfreertos.a:tasks.*(.literal.__getreent .literal.pcTaskGetName .literal.prvAddCurrentTaskToDelayedList .literal.prvDeleteTLS .literal.prvResetNextTaskUnblockTime .literal.prvTaskIsTaskSuspended .literal.pvTaskIncrementMutexHeldCount .literal.taskSelectHighestPriorityTaskSMP .literal.taskYIELD_OTHER_CORE .literal.ulTaskGenericNotifyValueClear .literal.uxTaskPriorityGetFromISR .literal.vTaskGenericNotifyGiveFromISR .literal.vTaskGetSnapshot .literal.vTaskInternalSetTimeOutState .literal.vTaskPlaceOnEventList .literal.vTaskPlaceOnEventListRestricted .literal.vTaskPlaceOnUnorderedEventList .literal.vTaskPriorityDisinheritAfterTimeout .literal.vTaskReleaseEventListLock .literal.vTaskSetTimeOutState .literal.vTaskSwitchContext .literal.vTaskTakeEventListLock .literal.xTaskAbortDelay .literal.xTaskCatchUpTicks .literal.xTaskCheckForTimeOut .literal.xTaskCreateRestrictedStatic .literal.xTaskGenericNotifyFromISR .literal.xTaskGenericNotifyStateClear .literal.xTaskGetCurrentTaskHandle .literal.xTaskGetHandle .literal.xTaskGetSchedulerState .literal.xTaskGetTickCount .literal.xTaskGetTickCountFromISR .literal.xTaskIncrementTick .literal.xTaskIncrementTickOtherCores .literal.xTaskPriorityDisinherit .literal.xTaskPriorityInherit .literal.xTaskRemoveFromEventList .literal.xTaskResumeFromISR .text .text.__getreent .text.pcTaskGetName .text.prvAddCurrentTaskToDelayedList .text.prvDeleteTLS .text.prvResetNextTaskUnblockTime .text.prvSearchForNameWithinSingleList .text.prvTaskIsTaskSuspended .text.pvTaskIncrementMutexHeldCount .text.taskSelectHighestPriorityTaskSMP .text.taskYIELD_OTHER_CORE .text.ulTaskGenericNotifyValueClear .text.uxTaskPriorityGetFromISR .text.vTaskGenericNotifyGiveFromISR .text.vTaskGetSnapshot .text.vTaskInternalSetTimeOutState .text.vTaskPlaceOnEventList .text.vTaskPlaceOnEventListRestricted .text.vTaskPlaceOnUnorderedEventList .text.vTaskPriorityDisinheritAfterTimeout .text.vTaskReleaseEventListLock .text.vTaskSetTimeOutState .text.vTaskSwitchContext .text.vTaskTakeEventListLock .text.xTaskAbortDelay .text.xTaskCatchUpTicks .text.xTaskCheckForTimeOut .text.xTaskCreateRestrictedStatic .text.xTaskGenericNotifyFromISR .text.xTaskGenericNotifyStateClear .text.xTaskGetCurrentTaskHandle .text.xTaskGetHandle .text.xTaskGetSchedulerState .text.xTaskGetTickCount .text.xTaskGetTickCountFromISR .text.xTaskIncrementTick .text.xTaskIncrementTickOtherCores .text.xTaskPriorityDisinherit .text.xTaskPriorityInherit .text.xTaskRemoveFromEventList .text.xTaskResumeFromISR .text.xTimerCreateTimerTask)
    *libfreertos.a:timers.*(.literal.pcTimerGetName .literal.uxTimerGetReloadMode .literal.vTimerSetReloadMode .literal.xTimerGetTimerDaemonTaskHandle .literal.xTimerPendFunctionCall .literal.xTimerPendFunctionCallFromISR .text .text.pcTimerGetName .text.uxTimerGetReloadMode .text.vTimerSetReloadMode .text.xTimerGetTimerDaemonTaskHandle .text.xTimerPendFunctionCall .text.xTimerPendFunctionCallFromISR)
    *libgcc.a:lib2funcs.*(.literal .literal.* .text .text.*)
    *libgcov.a:(.literal .literal.* .text .text.*)
    *libhal.a:i2c_hal_iram.*(.literal .literal.* .text .text.*)
    *libhal.a:ledc_hal_iram.*(.literal .literal.* .text .text.*)
    *libhal.a:mmu_hal.*(.literal .literal.* .text .text.*)
    *libhal.a:spi_flash_encrypt_hal_iram.*(.literal .literal.* .text .text.*)
    *libhal.a:spi_flash_hal_iram.*(.literal .literal.* .text .text.*)
    *libhal.a:spi_hal_iram.*(.literal .literal.* .text .text.*)
    *libhal.a:spi_slave_hal_iram.*(.literal .literal.* .text .text.*)
    *libhal.a:wdt_hal_iram.*(.literal .literal.* .text .text.*)
    *libheap.a:multi_heap.*(.literal.assert_valid_block .text.assert_valid_block)
    *libheap.a:multi_heap.*(.literal.multi_heap_aligned_alloc_impl .text.multi_heap_aligned_alloc_impl)
    *libheap.a:multi_heap.*(.literal.multi_heap_aligned_alloc_impl_offs .text.multi_heap_aligned_alloc_impl_offs)
    *libheap.a:multi_heap.*(.literal.multi_heap_free_impl .text.multi_heap_free_impl)
    *libheap.a:multi_heap.*(.literal.multi_heap_get_allocated_size_impl .text.multi_heap_get_allocated_size_impl)
    *libheap.a:multi_heap.*(.literal.multi_heap_get_block_address_impl .text.multi_heap_get_block_address_impl)
    *libheap.a:multi_heap.*(.literal.multi_heap_get_first_block .text.multi_heap_get_first_block)
    *libheap.a:multi_heap.*(.literal.multi_heap_get_next_block .text.multi_heap_get_next_block)
    *libheap.a:multi_heap.*(.literal.multi_heap_internal_lock .text.multi_heap_internal_lock)
    *libheap.a:multi_heap.*(.literal.multi_heap_internal_unlock .text.multi_heap_internal_unlock)
    *libheap.a:multi_heap.*(.literal.multi_heap_is_free .text.multi_heap_is_free)
    *libheap.a:multi_heap.*(.literal.multi_heap_malloc_impl .text.multi_heap_malloc_impl)
    *libheap.a:multi_heap.*(.literal.multi_heap_realloc_impl .text.multi_heap_realloc_impl)
    *libheap.a:multi_heap.*(.literal.multi_heap_set_lock .text.multi_heap_set_lock)
    *libheap.a:tlsf.*(.literal.tlsf_align_size .text.tlsf_align_size)
    *libheap.a:tlsf.*(.literal.tlsf_alloc_overhead .text.tlsf_alloc_overhead)
    *libheap.a:tlsf.*(.literal.tlsf_block_size .text.tlsf_block_size)
    *libheap.a:tlsf.*(.literal.tlsf_block_size_max .text.tlsf_block_size_max)
    *libheap.a:tlsf.*(.literal.tlsf_block_size_min .text.tlsf_block_size_min)
    *libheap.a:tlsf.*(.literal.tlsf_free .text.tlsf_free)
    *libheap.a:tlsf.*(.literal.tlsf_get_pool .text.tlsf_get_pool)
    *libheap.a:tlsf.*(.literal.tlsf_malloc .text.tlsf_malloc)
    *libheap.a:tlsf.*(.literal.tlsf_memalign .text.tlsf_memalign)
    *libheap.a:tlsf.*(.literal.tlsf_memalign_offs .text.tlsf_memalign_offs)
    *libheap.a:tlsf.*(.literal.tlsf_realloc .text.tlsf_realloc)
    *libheap.a:tlsf.*(.literal.tlsf_size .text.tlsf_size)
    *liblog.a:log.*(.literal.esp_log_write .text.esp_log_write)
    *liblog.a:log_freertos.*(.literal.esp_log_early_timestamp .text.esp_log_early_timestamp)
    *liblog.a:log_freertos.*(.literal.esp_log_impl_lock .text.esp_log_impl_lock)
    *liblog.a:log_freertos.*(.literal.esp_log_impl_lock_timeout .text.esp_log_impl_lock_timeout)
    *liblog.a:log_freertos.*(.literal.esp_log_impl_unlock .text.esp_log_impl_unlock)
    *liblog.a:log_freertos.*(.literal.esp_log_timestamp .text.esp_log_timestamp)
    *libnet80211.a:(.wifislpiram .wifislpiram.*)
    *libnet80211.a:(.wifislprxiram .wifislprxiram.*)
    *libnewlib.a:abort.*(.literal .literal.* .text .text.*)
    *libnewlib.a:assert.*(.literal .literal.* .text .text.*)
    *libnewlib.a:heap.*(.literal .literal.* .text .text.*)
    *libnewlib.a:stdatomic.*(.literal .literal.* .text .text.*)
    *libphy.a:(.phyiram .phyiram.*)
    *libpp.a:(.wifiorslpiram .wifiorslpiram.*)
    *libpp.a:(.wifislpiram .wifislpiram.*)
    *libpp.a:(.wifislprxiram .wifislprxiram.*)
    *librtc.a:(.literal .literal.* .text .text.*)
    *libsoc.a:lldesc.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:flash_brownout_hook.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:memspi_host_driver.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_boya.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_gd.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_generic.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_issi.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_mxic.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_th.*(.literal .literal.* .text .text.*)
    *libspi_flash.a:spi_flash_chip_winbond.*(.literal .literal.* .text .text.*)
    *libxt_hal.a:(.literal .literal.* .text .text.*)
    *libxtensa.a:eri.*(.literal .literal.* .text .text.*)
    *libxtensa.a:xtensa_intr_asm.*(.literal .literal.* .text .text.*)

  } > iram0_0_seg

  .dram0.data :
  {
    _data_start = ABSOLUTE(.);
    *(.gnu.linkonce.d.*)
    *(.data1)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    *(.gnu.linkonce.s2.*)
    *(.jcr)

    *(EXCLUDE_FILE(*libbt.a *libbtdm_app.a *libnimble.a) .data EXCLUDE_FILE(*libbt.a *libbtdm_app.a *libnimble.a) .data.*)
    *(.dram1 .dram1.*)
    _coredump_dram_start = ABSOLUTE(.);
    *(.dram2.coredump .dram2.coredump.*)
    _coredump_dram_end = ABSOLUTE(.);
    *libapp_trace.a:app_trace.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libapp_trace.a:app_trace_util.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libapp_trace.a:port_uart.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    . = ALIGN(4);
    _bt_data_start = ABSOLUTE(.);
    *libbt.a:(.data .data.*)
    . = ALIGN(4);
    _bt_data_end = ABSOLUTE(.);
    . = ALIGN(4);
    _btdm_data_start = ABSOLUTE(.);
    *libbtdm_app.a:(.data .data.*)
    . = ALIGN(4);
    _btdm_data_end = ABSOLUTE(.);
    *libesp_hw_support.a:esp_memory_utils.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libesp_hw_support.a:rtc_clk.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libesp_psram.a:mmu.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libesp_rom.a:esp_rom_regi2c.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libesp_rom.a:esp_rom_spiflash.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libesp_system.a:esp_err.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libesp_system.a:ubsan.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libgcov.a:(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:i2c_hal_iram.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:ledc_hal_iram.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:mmu_hal.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:spi_flash_encrypt_hal_iram.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:spi_flash_hal_iram.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:spi_hal_iram.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:spi_slave_hal_iram.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libhal.a:wdt_hal_iram.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libnewlib.a:abort.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libnewlib.a:assert.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libnewlib.a:heap.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libnewlib.a:stdatomic.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    . = ALIGN(4);
    _nimble_data_start = ABSOLUTE(.);
    *libnimble.a:(.data .data.*)
    . = ALIGN(4);
    _nimble_data_end = ABSOLUTE(.);
    *libphy.a:(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libsoc.a:lldesc.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libspi_flash.a:flash_brownout_hook.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libspi_flash.a:memspi_host_driver.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libspi_flash.a:spi_flash_chip_boya.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libspi_flash.a:spi_flash_chip_gd.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libspi_flash.a:spi_flash_chip_generic.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libspi_flash.a:spi_flash_chip_issi.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libspi_flash.a:spi_flash_chip_mxic.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libspi_flash.a:spi_flash_chip_th.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)
    *libspi_flash.a:spi_flash_chip_winbond.*(.rodata .rodata.* .sdata2 .sdata2.* .srodata .srodata.*)

    _data_end = ABSOLUTE(.);
    . = ALIGN(4);
  } > dram0_0_seg

  /**
   * This section holds data that won't be initialised when startup.
   * This section locates in External RAM region.
   */
  .ext_ram_noinit (NOLOAD) :
  {
    _ext_ram_noinit_start = ABSOLUTE(.);
    *(.ext_ram_noinit*)
    . = ALIGN(4);
    _ext_ram_noinit_end = ABSOLUTE(.);
  } > extern_ram_seg

  /*This section holds data that should not be initialized at power up.
    The section located in Internal SRAM memory region. The macro _NOINIT
    can be used as attribute to place data into this section.
    See the esp_attr.h file for more information.
  */
  .noinit (NOLOAD):
  {
    . = ALIGN(4);
    _noinit_start = ABSOLUTE(.);
    *(.noinit .noinit.*)
    . = ALIGN(4) ;
    _noinit_end = ABSOLUTE(.);
  } > dram0_0_seg

   /* external memory bss, from any global variable with EXT_RAM_BSS_ATTR attribute*/
  .ext_ram.bss (NOLOAD) :
  {
    _ext_ram_bss_start = ABSOLUTE(.);


    . = ALIGN(4);
    _ext_ram_bss_end = ABSOLUTE(.);
  } > extern_ram_seg

  /* Shared RAM */
  .dram0.bss (NOLOAD) :
  {
    . = ALIGN (8);
    _bss_start = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libbt.a *libbtdm_app.a *libnimble.a) .bss EXCLUDE_FILE(*libbt.a *libbtdm_app.a *libnimble.a) .bss.*)
    *(.dynbss .dynsbss .gnu.linkonce.b .gnu.linkonce.b.* .gnu.linkonce.sb .gnu.linkonce.sb.* .gnu.linkonce.sb2 .gnu.linkonce.sb2.* .sbss .sbss.* .sbss2 .sbss2.* .scommon .share.mem)
    *(.ext_ram.bss .ext_ram.bss.*)
    *(EXCLUDE_FILE(*libbt.a *libbtdm_app.a *libnimble.a) COMMON)
    . = ALIGN(4);
    _bt_bss_start = ABSOLUTE(.);
    *libbt.a:(.bss .bss.*)
    . = ALIGN(4);
    _bt_bss_end = ABSOLUTE(.);
    . = ALIGN(4);
    _bt_common_start = ABSOLUTE(.);
    *libbt.a:(COMMON)
    . = ALIGN(4);
    _bt_common_end = ABSOLUTE(.);
    . = ALIGN(4);
    _btdm_bss_start = ABSOLUTE(.);
    *libbtdm_app.a:(.bss .bss.*)
    . = ALIGN(4);
    _btdm_bss_end = ABSOLUTE(.);
    . = ALIGN(4);
    _btdm_common_start = ABSOLUTE(.);
    *libbtdm_app.a:(COMMON)
    . = ALIGN(4);
    _btdm_common_end = ABSOLUTE(.);
    . = ALIGN(4);
    _nimble_bss_start = ABSOLUTE(.);
    *libnimble.a:(.bss .bss.*)
    . = ALIGN(4);
    _nimble_bss_end = ABSOLUTE(.);
    . = ALIGN(4);
    _nimble_common_start = ABSOLUTE(.);
    *libnimble.a:(COMMON)
    . = ALIGN(4);
    _nimble_common_end = ABSOLUTE(.);

    . = ALIGN (8);
    _bss_end = ABSOLUTE(.);
  } > dram0_0_seg

  ASSERT(((_bss_end - ORIGIN(dram0_0_seg)) <= LENGTH(dram0_0_seg)),
          "DRAM segment data does not fit.")

  .flash.appdesc : ALIGN(0x10)
  {
    _rodata_reserved_start = ABSOLUTE(.);  /* This is a symbol marking the flash.rodata start, this can be used for mmu driver to maintain virtual address */
    _rodata_start = ABSOLUTE(.);

    *(.rodata_desc .rodata_desc.*)               /* Should be the first.  App version info.        DO NOT PUT ANYTHING BEFORE IT! */
    *(.rodata_custom_desc .rodata_custom_desc.*) /* Should be the second. Custom app version info. DO NOT PUT ANYTHING BEFORE IT! */

    /* Create an empty gap within this section. Thanks to this, the end of this
     * section will match .flah.rodata's begin address. Thus, both sections
     * will be merged when creating the final bin image. */
    . = ALIGN(ALIGNOF(.flash.rodata));
  } >default_rodata_seg

  .flash.rodata : ALIGN(0x10)
  {
    _flash_rodata_start = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libgcov.a *libphy.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:port_uart.* *libesp_hw_support.a:esp_memory_utils.* *libesp_hw_support.a:rtc_clk.* *libesp_psram.a:mmu.* *libesp_rom.a:esp_rom_regi2c.* *libesp_rom.a:esp_rom_spiflash.* *libesp_system.a:esp_err.* *libesp_system.a:ubsan.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:mmu_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:flash_brownout_hook.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.*) .rodata EXCLUDE_FILE(*libgcov.a *libphy.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:port_uart.* *libesp_hw_support.a:esp_memory_utils.* *libesp_hw_support.a:rtc_clk.* *libesp_psram.a:mmu.* *libesp_rom.a:esp_rom_regi2c.* *libesp_rom.a:esp_rom_spiflash.* *libesp_system.a:esp_err.* *libesp_system.a:ubsan.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:mmu_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:flash_brownout_hook.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.*) .rodata.* EXCLUDE_FILE(*libgcov.a *libphy.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:port_uart.* *libesp_hw_support.a:esp_memory_utils.* *libesp_hw_support.a:rtc_clk.* *libesp_psram.a:mmu.* *libesp_rom.a:esp_rom_regi2c.* *libesp_rom.a:esp_rom_spiflash.* *libesp_system.a:esp_err.* *libesp_system.a:ubsan.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:mmu_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:flash_brownout_hook.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.*) .sdata2 EXCLUDE_FILE(*libgcov.a *libphy.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:port_uart.* *libesp_hw_support.a:esp_memory_utils.* *libesp_hw_support.a:rtc_clk.* *libesp_psram.a:mmu.* *libesp_rom.a:esp_rom_regi2c.* *libesp_rom.a:esp_rom_spiflash.* *libesp_system.a:esp_err.* *libesp_system.a:ubsan.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:mmu_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:flash_brownout_hook.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.*) .sdata2.* EXCLUDE_FILE(*libgcov.a *libphy.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:port_uart.* *libesp_hw_support.a:esp_memory_utils.* *libesp_hw_support.a:rtc_clk.* *libesp_psram.a:mmu.* *libesp_rom.a:esp_rom_regi2c.* *libesp_rom.a:esp_rom_spiflash.* *libesp_system.a:esp_err.* *libesp_system.a:ubsan.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:mmu_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:flash_brownout_hook.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.*) .srodata EXCLUDE_FILE(*libgcov.a *libphy.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:port_uart.* *libesp_hw_support.a:esp_memory_utils.* *libesp_hw_support.a:rtc_clk.* *libesp_psram.a:mmu.* *libesp_rom.a:esp_rom_regi2c.* *libesp_rom.a:esp_rom_spiflash.* *libesp_system.a:esp_err.* *libesp_system.a:ubsan.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:mmu_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:flash_brownout_hook.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.*) .srodata.*)
    *(.rodata_wlog_error .rodata_wlog_error.*)


    *(.irom1.text) /* catch stray ICACHE_RODATA_ATTR */
    *(.gnu.linkonce.r.*)
    *(.rodata1)
    __XT_EXCEPTION_TABLE_ = ABSOLUTE(.);
    *(.xt_except_table)
    *(.gcc_except_table .gcc_except_table.*)
    *(.gnu.linkonce.e.*)
    *(.gnu.version_r)
    . = (. + 3) & ~ 3;
    __eh_frame = ABSOLUTE(.);
    KEEP(*(.eh_frame))
    . = (. + 7) & ~ 3;
    /*  C++ constructor and destructor tables

        Make a point of not including anything from crtbegin.o or crtend.o, as IDF doesn't use toolchain crt
      */
    __init_array_start = ABSOLUTE(.);
    KEEP (*(EXCLUDE_FILE (*crtend.* *crtbegin.*) .ctors SORT(.ctors.*)))
    __init_array_end = ABSOLUTE(.);

    KEEP (*crtbegin.*(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.*) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    /*  C++ exception handlers table:  */
    __XT_EXCEPTION_DESCS_ = ABSOLUTE(.);
    *(.xt_except_desc)
    *(.gnu.linkonce.h.*)
    __XT_EXCEPTION_DESCS_END__ = ABSOLUTE(.);
    *(.xt_except_desc_end)
    *(.dynamic)
    *(.gnu.version_d)
    /* Addresses of memory regions reserved via
       SOC_RESERVE_MEMORY_REGION() */
    soc_reserved_memory_region_start = ABSOLUTE(.);
    KEEP (*(.reserved_memory_address))
    soc_reserved_memory_region_end = ABSOLUTE(.);
    /* System init functions registered via ESP_SYSTEM_INIT_FN */
    _esp_system_init_fn_array_start = ABSOLUTE(.);
    KEEP (*(SORT_BY_INIT_PRIORITY(.esp_system_init_fn.*)))
    _esp_system_init_fn_array_end = ABSOLUTE(.);
    _rodata_end = ABSOLUTE(.);
    /* Literals are also RO data. */
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.lit4.*)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
    . = ALIGN(4);
    _thread_local_start = ABSOLUTE(.);
    *(.tdata)
    *(.tdata.*)
    *(.tbss)
    *(.tbss.*)
    _thread_local_end = ABSOLUTE(.);
    _rodata_reserved_end = ABSOLUTE(.);  /* This is a symbol marking the flash.rodata end, this can be used for mmu driver to maintain virtual address */
    . = ALIGN(4);
  } >default_rodata_seg

  _flash_rodata_align = ALIGNOF(.flash.rodata);

  .flash.rodata_noload (NOLOAD) :
  {
    . = ALIGN (4);
    *(.rodata_wlog_debug .rodata_wlog_debug.*)
    *(.rodata_wlog_info .rodata_wlog_info.*)
    *(.rodata_wlog_verbose .rodata_wlog_verbose.*)
    *(.rodata_wlog_warning .rodata_wlog_warning.*)
  } > default_rodata_seg

  .flash.text :
  {
    _stext = .;
    _instruction_reserved_start = ABSOLUTE(.);  /* This is a symbol marking the flash.text start, this can be used for mmu driver to maintain virtual address */
    _text_start = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libesp_ringbuf.a *libfreertos.a *libgcov.a *librtc.a *libxt_hal.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:port_uart.* *libesp_event.a:default_event_loop.* *libesp_event.a:esp_event.* *libesp_hw_support.a:cpu.* *libesp_hw_support.a:esp_memory_utils.* *libesp_hw_support.a:periph_ctrl.* *libesp_hw_support.a:rtc_clk.* *libesp_hw_support.a:rtc_init.* *libesp_hw_support.a:rtc_pm.* *libesp_hw_support.a:rtc_sleep.* *libesp_hw_support.a:rtc_time.* *libesp_hw_support.a:rtc_wdt.* *libesp_phy.a:phy_init.* *libesp_psram.a:mmu.* *libesp_rom.a:esp_rom_regi2c.* *libesp_rom.a:esp_rom_spiflash.* *libesp_system.a:esp_err.* *libesp_system.a:esp_system.* *libesp_system.a:ubsan.* *libesp_timer.a:esp_timer.* *libesp_wifi.a:esp_adapter.* *libgcc.a:lib2funcs.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:mmu_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libheap.a:multi_heap.* *libheap.a:tlsf.* *liblog.a:log.* *liblog.a:log_freertos.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:flash_brownout_hook.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.* *libxtensa.a:eri.* *libxtensa.a:xtensa_intr_asm.*) .literal EXCLUDE_FILE(*libesp_ringbuf.a *libfreertos.a *libgcov.a *librtc.a *libxt_hal.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:port_uart.* *libesp_event.a:default_event_loop.* *libesp_event.a:esp_event.* *libesp_hw_support.a:cpu.* *libesp_hw_support.a:esp_memory_utils.* *libesp_hw_support.a:periph_ctrl.* *libesp_hw_support.a:rtc_clk.* *libesp_hw_support.a:rtc_init.* *libesp_hw_support.a:rtc_pm.* *libesp_hw_support.a:rtc_sleep.* *libesp_hw_support.a:rtc_time.* *libesp_hw_support.a:rtc_wdt.* *libesp_phy.a:phy_init.* *libesp_psram.a:mmu.* *libesp_rom.a:esp_rom_regi2c.* *libesp_rom.a:esp_rom_spiflash.* *libesp_system.a:esp_err.* *libesp_system.a:esp_system.* *libesp_system.a:ubsan.* *libesp_timer.a:esp_timer.* *libesp_wifi.a:esp_adapter.* *libgcc.a:lib2funcs.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:mmu_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libheap.a:multi_heap.* *libheap.a:tlsf.* *liblog.a:log.* *liblog.a:log_freertos.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:flash_brownout_hook.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.* *libxtensa.a:eri.* *libxtensa.a:xtensa_intr_asm.*) .literal.* EXCLUDE_FILE(*libesp_ringbuf.a *libfreertos.a *libgcov.a *librtc.a *libxt_hal.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:port_uart.* *libesp_event.a:default_event_loop.* *libesp_event.a:esp_event.* *libesp_hw_support.a:cpu.* *libesp_hw_support.a:esp_memory_utils.* *libesp_hw_support.a:periph_ctrl.* *libesp_hw_support.a:rtc_clk.* *libesp_hw_support.a:rtc_init.* *libesp_hw_support.a:rtc_pm.* *libesp_hw_support.a:rtc_sleep.* *libesp_hw_support.a:rtc_time.* *libesp_hw_support.a:rtc_wdt.* *libesp_phy.a:phy_init.* *libesp_psram.a:mmu.* *libesp_rom.a:esp_rom_regi2c.* *libesp_rom.a:esp_rom_spiflash.* *libesp_system.a:esp_err.* *libesp_system.a:esp_system.* *libesp_system.a:ubsan.* *libesp_timer.a:esp_timer.* *libesp_wifi.a:esp_adapter.* *libgcc.a:lib2funcs.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:mmu_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libheap.a:multi_heap.* *libheap.a:tlsf.* *liblog.a:log.* *liblog.a:log_freertos.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:flash_brownout_hook.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.* *libxtensa.a:eri.* *libxtensa.a:xtensa_intr_asm.*) .text EXCLUDE_FILE(*libesp_ringbuf.a *libfreertos.a *libgcov.a *librtc.a *libxt_hal.a *libapp_trace.a:app_trace.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:port_uart.* *libesp_event.a:default_event_loop.* *libesp_event.a:esp_event.* *libesp_hw_support.a:cpu.* *libesp_hw_support.a:esp_memory_utils.* *libesp_hw_support.a:periph_ctrl.* *libesp_hw_support.a:rtc_clk.* *libesp_hw_support.a:rtc_init.* *libesp_hw_support.a:rtc_pm.* *libesp_hw_support.a:rtc_sleep.* *libesp_hw_support.a:rtc_time.* *libesp_hw_support.a:rtc_wdt.* *libesp_phy.a:phy_init.* *libesp_psram.a:mmu.* *libesp_rom.a:esp_rom_regi2c.* *libesp_rom.a:esp_rom_spiflash.* *libesp_system.a:esp_err.* *libesp_system.a:esp_system.* *libesp_system.a:ubsan.* *libesp_timer.a:esp_timer.* *libesp_wifi.a:esp_adapter.* *libgcc.a:lib2funcs.* *libhal.a:i2c_hal_iram.* *libhal.a:ledc_hal_iram.* *libhal.a:mmu_hal.* *libhal.a:spi_flash_encrypt_hal_iram.* *libhal.a:spi_flash_hal_iram.* *libhal.a:spi_hal_iram.* *libhal.a:spi_slave_hal_iram.* *libhal.a:wdt_hal_iram.* *libheap.a:multi_heap.* *libheap.a:tlsf.* *liblog.a:log.* *liblog.a:log_freertos.* *libnewlib.a:abort.* *libnewlib.a:assert.* *libnewlib.a:heap.* *libnewlib.a:stdatomic.* *libsoc.a:lldesc.* *libspi_flash.a:flash_brownout_hook.* *libspi_flash.a:memspi_host_driver.* *libspi_flash.a:spi_flash_chip_boya.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_mxic.* *libspi_flash.a:spi_flash_chip_th.* *libspi_flash.a:spi_flash_chip_winbond.* *libxtensa.a:eri.* *libxtensa.a:xtensa_intr_asm.*) .text.*)
    *(.wifi0iram .wifi0iram.*)
    *(EXCLUDE_FILE(*libpp.a) .wifiorslpiram EXCLUDE_FILE(*libpp.a) .wifiorslpiram.*)
    *(.wifirxiram .wifirxiram.*)
    *(EXCLUDE_FILE(*libcoexist.a *libnet80211.a *libpp.a) .wifislpiram EXCLUDE_FILE(*libcoexist.a *libnet80211.a *libpp.a) .wifislpiram.*)
    *(EXCLUDE_FILE(*libnet80211.a *libpp.a) .wifislprxiram EXCLUDE_FILE(*libnet80211.a *libpp.a) .wifislprxiram.*)
    *libesp_event.a:default_event_loop.*(.literal.esp_event_handler_instance_register .literal.esp_event_handler_instance_unregister .literal.esp_event_handler_register .literal.esp_event_handler_unregister .literal.esp_event_loop_create_default .literal.esp_event_loop_delete_default .literal.esp_event_post .text .text.esp_event_handler_instance_register .text.esp_event_handler_instance_unregister .text.esp_event_handler_register .text.esp_event_handler_unregister .text.esp_event_loop_create_default .text.esp_event_loop_delete_default .text.esp_event_post)
    *libesp_event.a:esp_event.*(.literal.base_node_add_handler .literal.base_node_remove_all_handler .literal.base_node_remove_handler .literal.esp_event_handler_instance_register_with .literal.esp_event_handler_instance_unregister_with .literal.esp_event_handler_register_with .literal.esp_event_handler_register_with_internal .literal.esp_event_handler_unregister_with .literal.esp_event_handler_unregister_with_internal .literal.esp_event_loop_create .literal.esp_event_loop_delete .literal.esp_event_loop_run .literal.esp_event_loop_run_task .literal.esp_event_post_to .literal.handler_instances_add .literal.handler_instances_remove .literal.handler_instances_remove_all .literal.loop_node_add_handler .literal.loop_node_remove_all_handler .literal.loop_node_remove_handler .text .text.base_node_add_handler .text.base_node_remove_all_handler .text.base_node_remove_handler .text.esp_event_dump .text.esp_event_handler_instance_register_with .text.esp_event_handler_instance_unregister_with .text.esp_event_handler_register_with .text.esp_event_handler_register_with_internal .text.esp_event_handler_unregister_with .text.esp_event_handler_unregister_with_internal .text.esp_event_loop_create .text.esp_event_loop_delete .text.esp_event_loop_run .text.esp_event_loop_run_task .text.esp_event_post_to .text.handler_execute .text.handler_instances_add .text.handler_instances_remove .text.handler_instances_remove_all .text.loop_node_add_handler .text.loop_node_remove_all_handler .text.loop_node_remove_handler)
    *libesp_hw_support.a:cpu.*(.literal.esp_cpu_configure_region_protection .literal.esp_cpu_intr_get_desc .literal.esp_cpu_set_watchpoint .text .text.esp_cpu_clear_breakpoint .text.esp_cpu_clear_watchpoint .text.esp_cpu_configure_region_protection .text.esp_cpu_intr_get_desc .text.esp_cpu_set_breakpoint .text.esp_cpu_set_watchpoint)
    *libesp_hw_support.a:periph_ctrl.*(.literal.periph_module_disable .literal.periph_module_enable .text .text.periph_module_disable .text.periph_module_enable)
    *libesp_hw_support.a:rtc_init.*(.literal.rtc_init .literal.rtc_vddsdio_get_config .text .text.rtc_init .text.rtc_vddsdio_get_config)
    *libesp_phy.a:phy_init.*(.literal.esp_phy_erase_cal_data_in_nvs .literal.esp_phy_get_init_data .literal.esp_phy_is_initialized .literal.esp_phy_load_cal_and_init .literal.esp_phy_load_cal_data_from_nvs .literal.esp_phy_modem_deinit .literal.esp_phy_modem_init .literal.esp_phy_rf_get_on_ts .literal.esp_phy_store_cal_data_to_nvs .literal.load_cal_data_from_nvs_handle .literal.store_cal_data_to_nvs_handle .text .text.esp_phy_erase_cal_data_in_nvs .text.esp_phy_get_init_data .text.esp_phy_is_initialized .text.esp_phy_load_cal_and_init .text.esp_phy_load_cal_data_from_nvs .text.esp_phy_modem_deinit .text.esp_phy_modem_init .text.esp_phy_reduce_tx_power .text.esp_phy_release_init_data .text.esp_phy_rf_get_on_ts .text.esp_phy_store_cal_data_to_nvs .text.esp_phy_update_country_info .text.load_cal_data_from_nvs_handle .text.store_cal_data_to_nvs_handle)
    *libesp_ringbuf.a:ringbuf.*(.literal.prvGetCurMaxSizeAllowSplit .text.prvGetCurMaxSizeAllowSplit)
    *libesp_ringbuf.a:ringbuf.*(.literal.prvGetCurMaxSizeByteBuf .text.prvGetCurMaxSizeByteBuf)
    *libesp_ringbuf.a:ringbuf.*(.literal.prvGetCurMaxSizeNoSplit .text.prvGetCurMaxSizeNoSplit)
    *libesp_ringbuf.a:ringbuf.*(.literal.prvInitializeNewRingbuffer .text.prvInitializeNewRingbuffer)
    *libesp_ringbuf.a:ringbuf.*(.literal.prvReceiveGeneric .text.prvReceiveGeneric)
    *libesp_ringbuf.a:ringbuf.*(.literal.vRingbufferDelete .text.vRingbufferDelete)
    *libesp_ringbuf.a:ringbuf.*(.literal.vRingbufferGetInfo .text.vRingbufferGetInfo)
    *libesp_ringbuf.a:ringbuf.*(.literal.vRingbufferReturnItem .text.vRingbufferReturnItem)
    *libesp_ringbuf.a:ringbuf.*(.literal.xRingbufferAddToQueueSetRead .text.xRingbufferAddToQueueSetRead)
    *libesp_ringbuf.a:ringbuf.*(.literal.xRingbufferCanRead .text.xRingbufferCanRead)
    *libesp_ringbuf.a:ringbuf.*(.literal.xRingbufferCreate .text.xRingbufferCreate)
    *libesp_ringbuf.a:ringbuf.*(.literal.xRingbufferCreateStatic .text.xRingbufferCreateStatic)
    *libesp_ringbuf.a:ringbuf.*(.literal.xRingbufferReceive .text.xRingbufferReceive)
    *libesp_ringbuf.a:ringbuf.*(.literal.xRingbufferReceiveSplit .text.xRingbufferReceiveSplit)
    *libesp_ringbuf.a:ringbuf.*(.literal.xRingbufferReceiveUpTo .text.xRingbufferReceiveUpTo)
    *libesp_ringbuf.a:ringbuf.*(.literal.xRingbufferRemoveFromQueueSetRead .text.xRingbufferRemoveFromQueueSetRead)
    *libesp_ringbuf.a:ringbuf.*(.literal.xRingbufferSend .text.xRingbufferSend)
    *libesp_system.a:esp_system.*(.literal.esp_get_free_heap_size .literal.esp_get_free_internal_heap_size .literal.esp_get_idf_version .literal.esp_get_minimum_free_heap_size .literal.esp_register_shutdown_handler .literal.esp_unregister_shutdown_handler .text .text.esp_get_free_heap_size .text.esp_get_free_internal_heap_size .text.esp_get_idf_version .text.esp_get_minimum_free_heap_size .text.esp_register_shutdown_handler .text.esp_unregister_shutdown_handler)
    *libesp_timer.a:esp_timer.*(.literal.__esp_system_init_fn_esp_timer_startup_init .literal.esp_timer_create .literal.esp_timer_deinit .literal.esp_timer_delete .literal.esp_timer_dump .literal.esp_timer_early_init .literal.esp_timer_init .literal.esp_timer_is_active .literal.esp_timer_restart .literal.print_timer_info .text .text.__esp_system_init_fn_esp_timer_startup_init .text.esp_timer_create .text.esp_timer_deinit .text.esp_timer_delete .text.esp_timer_dump .text.esp_timer_early_init .text.esp_timer_init .text.esp_timer_is_active .text.esp_timer_restart .text.print_timer_info)
    *libesp_wifi.a:esp_adapter.*(.literal.coex_condition_set_wrapper .literal.coex_deinit_wrapper .literal.coex_disable_wrapper .literal.coex_enable_wrapper .literal.coex_init_wrapper .literal.coex_register_start_cb_wrapper .literal.coex_schm_curr_period_get_wrapper .literal.coex_schm_curr_phase_get_wrapper .literal.coex_schm_curr_phase_idx_get_wrapper .literal.coex_schm_curr_phase_idx_set_wrapper .literal.coex_schm_interval_get_wrapper .literal.coex_schm_status_bit_clear_wrapper .literal.coex_schm_status_bit_set_wrapper .literal.coex_wifi_channel_set_wrapper .literal.coex_wifi_request_wrapper .literal.esp_cpu_intr_disable .literal.esp_cpu_intr_enable .literal.esp_event_post_wrapper .literal.event_group_wait_bits_wrapper .literal.get_time_wrapper .literal.internal_semphr_create_wrapper .literal.internal_semphr_delete_wrapper .literal.internal_semphr_give_wrapper .literal.internal_semphr_take_wrapper .literal.mutex_create_wrapper .literal.mutex_delete_wrapper .literal.queue_create_wrapper .literal.queue_recv_wrapper .literal.queue_send_to_back_wrapper .literal.queue_send_to_front_wrapper .literal.queue_send_wrapper .literal.recursive_mutex_create_wrapper .literal.semphr_create_wrapper .literal.semphr_delete_wrapper .literal.semphr_give_wrapper .literal.semphr_take_wrapper .literal.set_intr_wrapper .literal.set_isr_wrapper .literal.spin_lock_create_wrapper .literal.task_create_pinned_to_core_wrapper .literal.task_create_wrapper .literal.timer_done_wrapper .literal.timer_setfn_wrapper .literal.wifi_create_queue .literal.wifi_create_queue_wrapper .literal.wifi_delete_queue .literal.wifi_delete_queue_wrapper .literal.wifi_reset_mac_wrapper .literal.wifi_thread_semphr_free .literal.wifi_thread_semphr_get_wrapper .text .text.clear_intr_wrapper .text.coex_condition_set_wrapper .text.coex_deinit_wrapper .text.coex_disable_wrapper .text.coex_enable_wrapper .text.coex_init_wrapper .text.coex_pti_get_wrapper .text.coex_register_start_cb_wrapper .text.coex_schm_curr_period_get_wrapper .text.coex_schm_curr_phase_get_wrapper .text.coex_schm_curr_phase_idx_get_wrapper .text.coex_schm_curr_phase_idx_set_wrapper .text.coex_schm_interval_get_wrapper .text.coex_schm_status_bit_clear_wrapper .text.coex_schm_status_bit_set_wrapper .text.coex_wifi_channel_set_wrapper .text.coex_wifi_request_wrapper .text.esp_cpu_intr_disable .text.esp_cpu_intr_enable .text.esp_event_post_wrapper .text.event_group_wait_bits_wrapper .text.get_time_wrapper .text.internal_semphr_create_wrapper .text.internal_semphr_delete_wrapper .text.internal_semphr_give_wrapper .text.internal_semphr_take_wrapper .text.mutex_create_wrapper .text.mutex_delete_wrapper .text.queue_create_wrapper .text.queue_recv_wrapper .text.queue_send_to_back_wrapper .text.queue_send_to_front_wrapper .text.queue_send_wrapper .text.recursive_mutex_create_wrapper .text.semphr_create_wrapper .text.semphr_delete_wrapper .text.semphr_give_wrapper .text.semphr_take_wrapper .text.set_intr_wrapper .text.set_isr_wrapper .text.spin_lock_create_wrapper .text.task_create_pinned_to_core_wrapper .text.task_create_wrapper .text.task_get_max_priority_wrapper .text.timer_done_wrapper .text.timer_setfn_wrapper .text.wifi_create_queue .text.wifi_create_queue_wrapper .text.wifi_delete_queue .text.wifi_delete_queue_wrapper .text.wifi_reset_mac_wrapper .text.wifi_thread_semphr_free .text.wifi_thread_semphr_get_wrapper)
    *libfreertos.a:event_groups.*(.literal.prvTestWaitCondition .text.prvTestWaitCondition)
    *libfreertos.a:event_groups.*(.literal.vEventGroupDelete .text.vEventGroupDelete)
    *libfreertos.a:event_groups.*(.literal.xEventGroupClearBits .text.xEventGroupClearBits)
    *libfreertos.a:event_groups.*(.literal.xEventGroupCreate .text.xEventGroupCreate)
    *libfreertos.a:event_groups.*(.literal.xEventGroupCreateStatic .text.xEventGroupCreateStatic)
    *libfreertos.a:event_groups.*(.literal.xEventGroupSetBits .text.xEventGroupSetBits)
    *libfreertos.a:event_groups.*(.literal.xEventGroupSync .text.xEventGroupSync)
    *libfreertos.a:event_groups.*(.literal.xEventGroupWaitBits .text.xEventGroupWaitBits)
    *libfreertos.a:port.*(.literal.esp_startup_start_app .text.esp_startup_start_app)
    *libfreertos.a:port.*(.literal.esp_startup_start_app_other_cores .text.esp_startup_start_app_other_cores)
    *libfreertos.a:port.*(.literal.pxPortInitialiseStack .text.pxPortInitialiseStack)
    *libfreertos.a:port.*(.literal.vPortReleaseTaskMPUSettings .text.vPortReleaseTaskMPUSettings)
    *libfreertos.a:port.*(.literal.vPortStoreTaskMPUSettings .text.vPortStoreTaskMPUSettings)
    *libfreertos.a:port.*(.literal.xPortStartScheduler .text.xPortStartScheduler)
    *libfreertos.a:port_common.*(.literal.main_task .text.main_task)
    *libfreertos.a:queue.*(.literal.prvInitialiseMutex .text.prvInitialiseMutex)
    *libfreertos.a:queue.*(.literal.prvInitialiseNewQueue .text.prvInitialiseNewQueue)
    *libfreertos.a:queue.*(.literal.prvIsQueueFull .text.prvIsQueueFull)
    *libfreertos.a:queue.*(.literal.uxQueueMessagesWaiting .text.uxQueueMessagesWaiting)
    *libfreertos.a:queue.*(.literal.uxQueueSpacesAvailable .text.uxQueueSpacesAvailable)
    *libfreertos.a:queue.*(.literal.vQueueDelete .text.vQueueDelete)
    *libfreertos.a:queue.*(.literal.vQueueWaitForMessageRestricted .text.vQueueWaitForMessageRestricted)
    *libfreertos.a:queue.*(.literal.xQueueAddToSet .text.xQueueAddToSet)
    *libfreertos.a:queue.*(.literal.xQueueCreateCountingSemaphore .text.xQueueCreateCountingSemaphore)
    *libfreertos.a:queue.*(.literal.xQueueCreateCountingSemaphoreStatic .text.xQueueCreateCountingSemaphoreStatic)
    *libfreertos.a:queue.*(.literal.xQueueCreateMutex .text.xQueueCreateMutex)
    *libfreertos.a:queue.*(.literal.xQueueCreateMutexStatic .text.xQueueCreateMutexStatic)
    *libfreertos.a:queue.*(.literal.xQueueCreateSet .text.xQueueCreateSet)
    *libfreertos.a:queue.*(.literal.xQueueGenericCreate .text.xQueueGenericCreate)
    *libfreertos.a:queue.*(.literal.xQueueGenericCreateStatic .text.xQueueGenericCreateStatic)
    *libfreertos.a:queue.*(.literal.xQueueGenericReset .text.xQueueGenericReset)
    *libfreertos.a:queue.*(.literal.xQueueGenericSend .text.xQueueGenericSend)
    *libfreertos.a:queue.*(.literal.xQueueGetMutexHolder .text.xQueueGetMutexHolder)
    *libfreertos.a:queue.*(.literal.xQueueGiveMutexRecursive .text.xQueueGiveMutexRecursive)
    *libfreertos.a:queue.*(.literal.xQueueRemoveFromSet .text.xQueueRemoveFromSet)
    *libfreertos.a:queue.*(.literal.xQueueSelectFromSet .text.xQueueSelectFromSet)
    *libfreertos.a:queue.*(.literal.xQueueTakeMutexRecursive .text.xQueueTakeMutexRecursive)
    *libfreertos.a:tasks.*(.literal.eTaskGetState .text.eTaskGetState)
    *libfreertos.a:tasks.*(.literal.prvAddNewTaskToReadyList .text.prvAddNewTaskToReadyList)
    *libfreertos.a:tasks.*(.literal.prvCheckTasksWaitingTermination .text.prvCheckTasksWaitingTermination)
    *libfreertos.a:tasks.*(.literal.prvDeleteTCB .text.prvDeleteTCB)
    *libfreertos.a:tasks.*(.literal.prvIdleTask .text.prvIdleTask)
    *libfreertos.a:tasks.*(.literal.prvInitialiseNewTask .text.prvInitialiseNewTask)
    *libfreertos.a:tasks.*(.literal.prvInitialiseTaskLists .text.prvInitialiseTaskLists)
    *libfreertos.a:tasks.*(.literal.prvTaskCheckFreeStackSpace .text.prvTaskCheckFreeStackSpace)
    *libfreertos.a:tasks.*(.literal.prvTaskPriorityRaise .text.prvTaskPriorityRaise)
    *libfreertos.a:tasks.*(.literal.prvTaskPriorityRestore .text.prvTaskPriorityRestore)
    *libfreertos.a:tasks.*(.literal.pvTaskGetThreadLocalStoragePointer .text.pvTaskGetThreadLocalStoragePointer)
    *libfreertos.a:tasks.*(.literal.pxGetNextTaskList .text.pxGetNextTaskList)
    *libfreertos.a:tasks.*(.literal.pxTaskGetNext .text.pxTaskGetNext)
    *libfreertos.a:tasks.*(.literal.pxTaskGetStackStart .text.pxTaskGetStackStart)
    *libfreertos.a:tasks.*(.literal.ulTaskGenericNotifyTake .text.ulTaskGenericNotifyTake)
    *libfreertos.a:tasks.*(.literal.ulTaskNotifyTake .text.ulTaskNotifyTake)
    *libfreertos.a:tasks.*(.literal.uxTaskGetNumberOfTasks .text.uxTaskGetNumberOfTasks)
    *libfreertos.a:tasks.*(.literal.uxTaskGetSnapshotAll .text.uxTaskGetSnapshotAll)
    *libfreertos.a:tasks.*(.literal.uxTaskGetStackHighWaterMark .text.uxTaskGetStackHighWaterMark)
    *libfreertos.a:tasks.*(.literal.uxTaskPriorityGet .text.uxTaskPriorityGet)
    *libfreertos.a:tasks.*(.literal.uxTaskResetEventItemValue .text.uxTaskResetEventItemValue)
    *libfreertos.a:tasks.*(.literal.vTaskAllocateMPURegions .text.vTaskAllocateMPURegions)
    *libfreertos.a:tasks.*(.literal.vTaskDelay .text.vTaskDelay)
    *libfreertos.a:tasks.*(.literal.vTaskDelayUntil .text.vTaskDelayUntil)
    *libfreertos.a:tasks.*(.literal.vTaskDelete .text.vTaskDelete)
    *libfreertos.a:tasks.*(.literal.vTaskEndScheduler .text.vTaskEndScheduler)
    *libfreertos.a:tasks.*(.literal.vTaskMissedYield .text.vTaskMissedYield)
    *libfreertos.a:tasks.*(.literal.vTaskPrioritySet .text.vTaskPrioritySet)
    *libfreertos.a:tasks.*(.literal.vTaskRemoveFromUnorderedEventList .text.vTaskRemoveFromUnorderedEventList)
    *libfreertos.a:tasks.*(.literal.vTaskResume .text.vTaskResume)
    *libfreertos.a:tasks.*(.literal.vTaskSetThreadLocalStoragePointer .text.vTaskSetThreadLocalStoragePointer)
    *libfreertos.a:tasks.*(.literal.vTaskSetThreadLocalStoragePointerAndDelCallback .text.vTaskSetThreadLocalStoragePointerAndDelCallback)
    *libfreertos.a:tasks.*(.literal.vTaskStartScheduler .text.vTaskStartScheduler)
    *libfreertos.a:tasks.*(.literal.vTaskSuspend .text.vTaskSuspend)
    *libfreertos.a:tasks.*(.literal.vTaskSuspendAll .text.vTaskSuspendAll)
    *libfreertos.a:tasks.*(.literal.xTaskCreatePinnedToCore .text.xTaskCreatePinnedToCore)
    *libfreertos.a:tasks.*(.literal.xTaskCreateRestricted .text.xTaskCreateRestricted)
    *libfreertos.a:tasks.*(.literal.xTaskCreateStaticPinnedToCore .text.xTaskCreateStaticPinnedToCore)
    *libfreertos.a:tasks.*(.literal.xTaskDelayUntil .text.xTaskDelayUntil)
    *libfreertos.a:tasks.*(.literal.xTaskGenericNotify .text.xTaskGenericNotify)
    *libfreertos.a:tasks.*(.literal.xTaskGenericNotifyWait .text.xTaskGenericNotifyWait)
    *libfreertos.a:tasks.*(.literal.xTaskGetAffinity .text.xTaskGetAffinity)
    *libfreertos.a:tasks.*(.literal.xTaskGetCurrentTaskHandleForCPU .text.xTaskGetCurrentTaskHandleForCPU)
    *libfreertos.a:tasks.*(.literal.xTaskGetIdleTaskHandle .text.xTaskGetIdleTaskHandle)
    *libfreertos.a:tasks.*(.literal.xTaskGetIdleTaskHandleForCPU .text.xTaskGetIdleTaskHandleForCPU)
    *libfreertos.a:tasks.*(.literal.xTaskNotifyWait .text.xTaskNotifyWait)
    *libfreertos.a:tasks.*(.literal.xTaskResumeAll .text.xTaskResumeAll)
    *libfreertos.a:timers.*(.literal.prvCheckForValidListAndQueue .text.prvCheckForValidListAndQueue)
    *libfreertos.a:timers.*(.literal.prvGetNextExpireTime .text.prvGetNextExpireTime)
    *libfreertos.a:timers.*(.literal.prvInitialiseNewTimer .text.prvInitialiseNewTimer)
    *libfreertos.a:timers.*(.literal.prvInsertTimerInActiveList .text.prvInsertTimerInActiveList)
    *libfreertos.a:timers.*(.literal.prvProcessExpiredTimer .text.prvProcessExpiredTimer)
    *libfreertos.a:timers.*(.literal.prvProcessReceivedCommands .text.prvProcessReceivedCommands)
    *libfreertos.a:timers.*(.literal.prvProcessTimerOrBlockTask .text.prvProcessTimerOrBlockTask)
    *libfreertos.a:timers.*(.literal.prvSampleTimeNow .text.prvSampleTimeNow)
    *libfreertos.a:timers.*(.literal.prvSwitchTimerLists .text.prvSwitchTimerLists)
    *libfreertos.a:timers.*(.literal.prvTimerTask .text.prvTimerTask)
    *libfreertos.a:timers.*(.literal.pvTimerGetTimerID .text.pvTimerGetTimerID)
    *libfreertos.a:timers.*(.literal.vTimerSetTimerID .text.vTimerSetTimerID)
    *libfreertos.a:timers.*(.literal.xTimerCreate .text.xTimerCreate)
    *libfreertos.a:timers.*(.literal.xTimerCreateStatic .text.xTimerCreateStatic)
    *libfreertos.a:timers.*(.literal.xTimerCreateTimerTask .text.xTimerCreateTimerTask)
    *libfreertos.a:timers.*(.literal.xTimerGenericCommand .text.xTimerGenericCommand)
    *libfreertos.a:timers.*(.literal.xTimerGetExpiryTime .text.xTimerGetExpiryTime)
    *libfreertos.a:timers.*(.literal.xTimerGetPeriod .text.xTimerGetPeriod)
    *libfreertos.a:timers.*(.literal.xTimerIsTimerActive .text.xTimerIsTimerActive)
    *libheap.a:multi_heap.*(.literal.multi_heap_check .literal.multi_heap_dump .literal.multi_heap_dump_tlsf .literal.multi_heap_get_info_impl .literal.multi_heap_register_impl .text .text.multi_heap_check .text.multi_heap_dump .text.multi_heap_dump_tlsf .text.multi_heap_free_size_impl .text.multi_heap_get_block_owner .text.multi_heap_get_info_impl .text.multi_heap_get_info_tlsf .text.multi_heap_minimum_free_size_impl .text.multi_heap_register_impl)
    *libheap.a:tlsf.*(.literal.control_construct .literal.default_walker .literal.tlsf_add_pool .literal.tlsf_check .literal.tlsf_check_pool .literal.tlsf_create .literal.tlsf_create_with_pool .literal.tlsf_fit_size .literal.tlsf_remove_pool .literal.tlsf_walk_pool .text .text.control_construct .text.default_walker .text.integrity_walker .text.tlsf_add_pool .text.tlsf_check .text.tlsf_check_pool .text.tlsf_create .text.tlsf_create_with_pool .text.tlsf_destroy .text.tlsf_fit_size .text.tlsf_pool_overhead .text.tlsf_remove_pool .text.tlsf_walk_pool)
    *liblog.a:log.*(.literal.esp_log_level_get .literal.esp_log_level_set .literal.esp_log_set_vprintf .literal.esp_log_writev .literal.heap_bubble_down .literal.s_log_level_get_and_unlock .text .text.esp_log_level_get .text.esp_log_level_set .text.esp_log_set_vprintf .text.esp_log_writev .text.heap_bubble_down .text.s_log_level_get_and_unlock)
    *liblog.a:log_freertos.*(.literal.esp_log_system_timestamp .text .text.esp_log_system_timestamp)

    *(.stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *(.irom0.text) /* catch stray ICACHE_RODATA_ATTR */
    *(.fini.literal)
    *(.fini)
    *(.gnu.version)

    /** CPU will try to prefetch up to 16 bytes of
      * of instructions. This means that any configuration (e.g. MMU, PMS) must allow
      * safe access to up to 16 bytes after the last real instruction, add
      * dummy bytes to ensure this
      */
    . += _esp_flash_mmap_prefetch_pad_size;

    _text_end = ABSOLUTE(.);
    _instruction_reserved_end = ABSOLUTE(.);  /* This is a symbol marking the flash.text end, this can be used for mmu driver to maintain virtual address */
    _etext = .;

    /* Similar to _iram_start, this symbol goes here so it is
       resolved by addr2line in preference to the first symbol in
       the flash.text segment.
    */
    _flash_cache_start = ABSOLUTE(0);
  } >default_code_seg

  /* Marks the end of IRAM code segment */
  .iram0.text_end (NOLOAD) :
  {
    . = ALIGN (4);
    _iram_text_end = ABSOLUTE(.);
  } > iram0_0_seg

  .iram0.data :
  {
    . = ALIGN(4);
    _iram_data_start = ABSOLUTE(.);

    *(.iram.data .iram.data.*)
    _coredump_iram_start = ABSOLUTE(.);
    *(.iram2.coredump .iram2.coredump.*)
    _coredump_iram_end = ABSOLUTE(.);

    _iram_data_end = ABSOLUTE(.);
  } > iram0_0_seg

  .iram0.bss (NOLOAD) :
  {
    . = ALIGN(4);
    _iram_bss_start = ABSOLUTE(.);

    *(.iram.bss .iram.bss.*)

    _iram_bss_end = ABSOLUTE(.);
    . = ALIGN(4);
    _iram_end = ABSOLUTE(.);
   } > iram0_0_seg

  /* Marks the end of data, bss and possibly rodata  */
  .dram0.heap_start (NOLOAD) :
  {
    . = ALIGN (8);
    _heap_start = ABSOLUTE(.);
  } > dram0_0_seg

  /** This section will be used by the debugger and disassembler to get more information
    * about raw data present in the code.
    * Indeed, it may be required to add some padding at some points in the code
    * in order to align a branch/jump destination on a particular bound.
    * Padding these instructions will generate null bytes that shall be
    * interpreted as data, and not code by the debugger or disassembler.
    * This section will only be present in the ELF file, not in the final binary
    * For more details, check GCC-212
    */
  .xt.prop 0 :
  {
    KEEP (*(.xt.prop .gnu.linkonce.prop.*))
  }

  .xt.lit 0 :
  {
    KEEP (*(.xt.lit .gnu.linkonce.p.*))
  }
}

ASSERT(((_iram_end - ORIGIN(iram0_0_seg)) <= LENGTH(iram0_0_seg)),
          "IRAM0 segment data does not fit.")

ASSERT(((_heap_start - ORIGIN(dram0_0_seg)) <= LENGTH(dram0_0_seg)),
          "DRAM segment data does not fit.")
