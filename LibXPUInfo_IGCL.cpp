// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifdef XPUINFO_USE_IGCL

#include "LibXPUInfo.h"
#include "LibXPUInfo_EXT_IGCL.h"
#include "DebugStream.h"
#include "LibXPUInfo_Util.h"

namespace XI
{
#define RUN_IGCL_TELEMETRY_TESTS 0
#if RUN_IGCL_TELEMETRY_TESTS
#define PRINT_LOGS(...) printf(__VA_ARGS__)
	// Decoding the return code for the most common error codes.
	std::string DecodeRetCode(ctl_result_t Res)
	{
		switch (Res)
		{
		case CTL_RESULT_SUCCESS:
		{
			return std::string("[CTL_RESULT_SUCCESS]");
		}
		case CTL_RESULT_ERROR_CORE_OVERCLOCK_NOT_SUPPORTED:
		{
			return std::string("[CTL_RESULT_ERROR_CORE_OVERCLOCK_NOT_SUPPORTED]");
		}
		case CTL_RESULT_ERROR_CORE_OVERCLOCK_VOLTAGE_OUTSIDE_RANGE:
		{
			return std::string("[CTL_RESULT_ERROR_CORE_OVERCLOCK_VOLTAGE_OUTSIDE_RANGE]");
		}
		case CTL_RESULT_ERROR_CORE_OVERCLOCK_FREQUENCY_OUTSIDE_RANGE:
		{
			return std::string("[CTL_RESULT_ERROR_CORE_OVERCLOCK_FREQUENCY_OUTSIDE_RANGE]");
		}
		case CTL_RESULT_ERROR_CORE_OVERCLOCK_POWER_OUTSIDE_RANGE:
		{
			return std::string("[CTL_RESULT_ERROR_CORE_OVERCLOCK_POWER_OUTSIDE_RANGE]");
		}
		case CTL_RESULT_ERROR_CORE_OVERCLOCK_TEMPERATURE_OUTSIDE_RANGE:
		{
			return std::string("[CTL_RESULT_ERROR_CORE_OVERCLOCK_TEMPERATURE_OUTSIDE_RANGE]");
		}
		case CTL_RESULT_ERROR_GENERIC_START:
		{
			return std::string("[CTL_RESULT_ERROR_GENERIC_START]");
		}
		case CTL_RESULT_ERROR_CORE_OVERCLOCK_RESET_REQUIRED:
		{
			return std::string("[CTL_RESULT_ERROR_CORE_OVERCLOCK_RESET_REQUIRED]");
		}
		case CTL_RESULT_ERROR_CORE_OVERCLOCK_IN_VOLTAGE_LOCKED_MODE:
		{
			return std::string("[CTL_RESULT_ERROR_CORE_OVERCLOCK_IN_VOLTAGE_LOCKED_MODE");
		}
		case CTL_RESULT_ERROR_CORE_OVERCLOCK_WAIVER_NOT_SET:
		{
			return std::string("[CTL_RESULT_ERROR_CORE_OVERCLOCK_WAIVER_NOT_SET]");
		}
		case CTL_RESULT_ERROR_NOT_INITIALIZED:
		{
			return std::string("[CTL_RESULT_ERROR_NOT_INITIALIZED]");
		}
		case CTL_RESULT_ERROR_ALREADY_INITIALIZED:
		{
			return std::string("[CTL_RESULT_ERROR_ALREADY_INITIALIZED]");
		}
		case CTL_RESULT_ERROR_DEVICE_LOST:
		{
			return std::string("[CTL_RESULT_ERROR_DEVICE_LOST]");
		}
		case CTL_RESULT_ERROR_INSUFFICIENT_PERMISSIONS:
		{
			return std::string("[CTL_RESULT_ERROR_INSUFFICIENT_PERMISSIONS]");
		}
		case CTL_RESULT_ERROR_NOT_AVAILABLE:
		{
			return std::string("[CTL_RESULT_ERROR_NOT_AVAILABLE]");
		}
		case CTL_RESULT_ERROR_UNINITIALIZED:
		{
			return std::string("[CTL_RESULT_ERROR_UNINITIALIZED]");
		}
		case CTL_RESULT_ERROR_UNSUPPORTED_VERSION:
		{
			return std::string("[CTL_RESULT_ERROR_UNSUPPORTED_VERSION]");
		}
		case CTL_RESULT_ERROR_UNSUPPORTED_FEATURE:
		{
			return std::string("[CTL_RESULT_ERROR_UNSUPPORTED_FEATURE]");
		}
		case CTL_RESULT_ERROR_INVALID_ARGUMENT:
		{
			return std::string("[CTL_RESULT_ERROR_INVALID_ARGUMENT]");
		}
		case CTL_RESULT_ERROR_INVALID_NULL_HANDLE:
		{
			return std::string("[CTL_RESULT_ERROR_INVALID_NULL_HANDLE]");
		}
		case CTL_RESULT_ERROR_INVALID_NULL_POINTER:
		{
			return std::string("[CTL_RESULT_ERROR_INVALID_NULL_POINTER]");
		}
		case CTL_RESULT_ERROR_INVALID_SIZE:
		{
			return std::string("[CTL_RESULT_ERROR_INVALID_SIZE]");
		}
		case CTL_RESULT_ERROR_UNSUPPORTED_SIZE:
		{
			return std::string("[CTL_RESULT_ERROR_UNSUPPORTED_SIZE]");
		}
		case CTL_RESULT_ERROR_NOT_IMPLEMENTED:
		{
			return std::string("[CTL_RESULT_ERROR_NOT_IMPLEMENTED]");
		}
		case CTL_RESULT_ERROR_ZE_LOADER:
		{
			return std::string("[CTL_RESULT_ERROR_ZE_LOADER]");
		}
		case CTL_RESULT_ERROR_INVALID_OPERATION_TYPE:
		{
			return std::string("[CTL_RESULT_ERROR_INVALID_OPERATION_TYPE]");
		}
		case CTL_RESULT_ERROR_UNKNOWN:
		{
			return std::string("[CTL_RESULT_ERROR_UNKNOWN]");
		}
		default:
			return std::string("[Unknown Error]");
		}
	}

	/***************************************************************
	 * @brief
	 * place_holder_for_Detailed_desc
	 * @param
	 * @return
	 ***************************************************************/
	void CtlFrequencyTest(ctl_device_adapter_handle_t hDAhandle)
	{

		uint32_t FrequencyHandlerCount = 0;
		ctl_result_t res = ctlEnumFrequencyDomains(hDAhandle, &FrequencyHandlerCount, nullptr);
		if ((res != CTL_RESULT_SUCCESS) || FrequencyHandlerCount == 0)
		{
			PRINT_LOGS("\nTemperature component not supported. Error: %s", DecodeRetCode(res).c_str());
			return;
		}
		else
		{
			PRINT_LOGS("\nNumber of Frequency Handles [%d]", FrequencyHandlerCount);
		}

		ctl_freq_handle_t* pFrequencyHandle = new ctl_freq_handle_t[FrequencyHandlerCount];

		res = ctlEnumFrequencyDomains(hDAhandle, &FrequencyHandlerCount, pFrequencyHandle);

		if (res != CTL_RESULT_SUCCESS)
		{
			PRINT_LOGS("\nError: %s for Frequency handle.", DecodeRetCode(res).c_str());
			goto cleanUp;
		}

		for (uint32_t i = 0; i < FrequencyHandlerCount; i++)
		{
			PRINT_LOGS("\n\nFor Frequency Handle: [%d]", i);

			PRINT_LOGS("\n\n[Frequency] Properties:");

			ctl_freq_properties_t freqProperties = { 0 };
			freqProperties.Size = sizeof(ctl_freq_properties_t);
			res = ctlFrequencyGetProperties(pFrequencyHandle[i], &freqProperties);
			if (res)
			{
				PRINT_LOGS("\n from Frequency get properties. %s", DecodeRetCode(res).c_str());
			}
			else
			{
				PRINT_LOGS("\n[Frequency] Min [%f]] Mhz", freqProperties.min);
				PRINT_LOGS("\n[Frequency] Max [%f]] Mhz", freqProperties.max);
				PRINT_LOGS("\n[Frequency] Can Control Frequency? [%d]]", (uint32_t)freqProperties.canControl);
				PRINT_LOGS("\n[Frequency] Frequency Domain [%s]]", ((freqProperties.type == CTL_FREQ_DOMAIN_GPU) ? "GPU" : "MEMORY"));
			}

			PRINT_LOGS("\n\n[Frequency] State:");

			ctl_freq_state_t freqState = { 0 };
			freqState.Size = sizeof(ctl_freq_state_t);
			res = ctlFrequencyGetState(pFrequencyHandle[i], &freqState);
			if (res)
			{
				PRINT_LOGS("\n %s from Frequency get state.", DecodeRetCode(res).c_str());
			}
			else
			{
				PRINT_LOGS("\n[Frequency] Actual Frequency [%f] Mhz", freqState.actual);
				PRINT_LOGS("\n[Frequency] Efficient Frequency [%f] Mhz", freqState.efficient);
				PRINT_LOGS("\n[Frequency] Requested Frequency [%f] Mhz", freqState.request);
				PRINT_LOGS("\n[Frequency] Max Frequency at current TDP [%f] MHz", freqState.tdp);
				PRINT_LOGS("\n[Frequency] Throttle Reasons [%d]", freqState.throttleReasons);
				PRINT_LOGS("\n[Frequency] Voltage [%f] Volts", freqState.currentVoltage);
			}

			PRINT_LOGS("\n\n[Frequency] Get throttle time:");

			ctl_freq_throttle_time_t throttleTime = { 0 };
			throttleTime.Size = sizeof(ctl_freq_throttle_time_t);
			res = ctlFrequencyGetThrottleTime(pFrequencyHandle[i], &throttleTime);
			if (res)
			{
				PRINT_LOGS("\n %s from Frequency get throttle time.", DecodeRetCode(res).c_str());
			}
			else
			{
				PRINT_LOGS("\n[Frequency] Throttle Time [%I64u ] s", throttleTime.throttleTime);
				PRINT_LOGS("\n[Frequency] Timestamp [%I64u] s", throttleTime.timestamp);
			}

			PRINT_LOGS("\n\n[Frequency] Available clocks:");

			uint32_t numClocks = 0;
			res = ctlFrequencyGetAvailableClocks(pFrequencyHandle[i], &numClocks, 0);

			if (res || numClocks == 0)
			{
				PRINT_LOGS("\n %s from Frequency get available clocks.", DecodeRetCode(res).c_str());
			}
			else
			{
				PRINT_LOGS("\n[Frequency] Number of Available clocks [%d]", numClocks);

				//clocks = new double[numClocks];

				//res = ctlFrequencyGetAvailableClocks(pFrequencyHandle[i], &numClocks, clocks);

				//for (uint32_t i = 0; i < numClocks; i++)
				//{
				//	PRINT_LOGS("\n[Frequency] Clock [%d]  Freq[%f] Mhz", i, clocks[i]);
				//}

				//delete[] clocks;
				//clocks = nullptr;
			}

			PRINT_LOGS("\n\n[Frequency] Frequency range:");

			ctl_freq_range_t freqRange = { 0 };
			freqRange.Size = sizeof(ctl_freq_range_t);
			res = ctlFrequencyGetRange(pFrequencyHandle[i], &freqRange);

			if (res)
			{
				PRINT_LOGS("\n %s from Frequency get range.", DecodeRetCode(res).c_str());
			}
			else
			{
				PRINT_LOGS("\n[Frequency] Range Max [%f] Mhz", freqRange.max);
				PRINT_LOGS("\n[Frequency] Range Min [%f] Mhz", freqRange.min);
			}

			PRINT_LOGS("\n\n[Frequency] Set Frequency range:");

			res = ctlFrequencySetRange(pFrequencyHandle[i], &freqRange);

			if (res)
			{
				PRINT_LOGS("\n %s from Frequency set range.", DecodeRetCode(res).c_str());
			}
			else
			{

				PRINT_LOGS("\n\n[Frequency] Success for Set Range");
			}
		}

	cleanUp:
		delete[] pFrequencyHandle;
		pFrequencyHandle = nullptr;
	}

	void CtlPowerTest(ctl_device_adapter_handle_t hDAhandle)
	{
		uint32_t PowerHandlerCount = 0;
		ctl_result_t res = ctlEnumPowerDomains(hDAhandle, &PowerHandlerCount, nullptr);
		if ((res != CTL_RESULT_SUCCESS) || PowerHandlerCount == 0)
		{
			PRINT_LOGS("\nPower component not supported. Error: %s", DecodeRetCode(res).c_str());
			return;
		}
		else
		{
			PRINT_LOGS("\nNumber of Power Handles [%d]", PowerHandlerCount);
		}

		ctl_pwr_handle_t* pPowerHandle = new ctl_pwr_handle_t[PowerHandlerCount];

		res = ctlEnumPowerDomains(hDAhandle, &PowerHandlerCount, pPowerHandle);

		if (res != CTL_RESULT_SUCCESS)
		{
			PRINT_LOGS("\nError: %s for Power handle.", DecodeRetCode(res).c_str());
			goto cleanUp;
		}

		for (uint32_t i = 0; i < PowerHandlerCount; i++)
		{
			PRINT_LOGS("\n\nFor Power Handle [%d]", i);

			ctl_power_properties_t properties = { 0 };
			properties.Size = sizeof(ctl_power_properties_t);
			res = ctlPowerGetProperties(pPowerHandle[i], &properties);

			PRINT_LOGS("\n\n[Power] Properties:");

			if (res != CTL_RESULT_SUCCESS)
			{
				PRINT_LOGS("\n %s from Power get properties.", DecodeRetCode(res).c_str());
			}
			else
			{
				PRINT_LOGS("\n[Power] Can Control [%d]", (uint32_t)properties.canControl);
				PRINT_LOGS("\n[Power] Max Power Limit [%d] mW", properties.maxLimit);
				PRINT_LOGS("\n[Power] Min Power Limit [%d] mW", properties.minLimit);
			}

			PRINT_LOGS("\n\n[Power] Energy counter:");

			ctl_power_energy_counter_t energyCounter = { 0 };
			energyCounter.Size = sizeof(ctl_power_energy_counter_t);
			res = ctlPowerGetEnergyCounter(pPowerHandle[i], &energyCounter);

			if (res != CTL_RESULT_SUCCESS)
			{
				PRINT_LOGS("\n %s from Power get energy counter.", DecodeRetCode(res).c_str());
			}
			else
			{
				PRINT_LOGS("\n[Power] Energy Counter [%I64u] micro J", energyCounter.energy);
				PRINT_LOGS("\n[Power] Time Stamp [%I64u] time stamp", energyCounter.timestamp);
			}

			PRINT_LOGS("\n\n[Power] Get Limits:");

			ctl_power_limits_t powerLimits = {};
			powerLimits.Size = sizeof(ctl_power_limits_t);
			res = ctlPowerGetLimits(pPowerHandle[i], &powerLimits); // TODO: Partially works for DG2

			if (res != CTL_RESULT_SUCCESS)
			{
				PRINT_LOGS("\n %s from Power get limits.", DecodeRetCode(res).c_str());
			}
			else
			{
				PRINT_LOGS("\n[Power] Sustained Power Limit Enabled [%d]", (uint32_t)powerLimits.sustainedPowerLimit.enabled);
				PRINT_LOGS("\n[Power] Sustained Power (PL1) Value [%d] mW", powerLimits.sustainedPowerLimit.power);
				PRINT_LOGS("\n[Power] Sustained Power (PL1 Tau) Time Window [%d] ms", powerLimits.sustainedPowerLimit.interval);
				PRINT_LOGS("\n[Power] Burst Power Limit Enabled [%d]", (uint32_t)powerLimits.burstPowerLimit.enabled);
				PRINT_LOGS("\n[Power] Burst Power (PL2) Value [%d] mW", powerLimits.burstPowerLimit.power);
				PRINT_LOGS("\n[Power] Peak Power (PL4) AC Value [%d] mW", powerLimits.peakPowerLimits.powerAC);
				PRINT_LOGS("\n[Power] Peak Power (PL4) DC Value [%d] mW", powerLimits.peakPowerLimits.powerDC);
			}

			PRINT_LOGS("\n\n[Power] Set Limits:");

			res = ctlPowerSetLimits(pPowerHandle[i], &powerLimits);

			if (res != CTL_RESULT_SUCCESS)
			{
				PRINT_LOGS("\n %s from Power set limits.", DecodeRetCode(res).c_str());
			}
			else
			{
				PRINT_LOGS("\n\n[Power] Set Limits Success!");
			}
		}

	cleanUp:
		delete[] pPowerHandle;
		pPowerHandle = nullptr;
	}

	/***************************************************************
	 * @brief
	 * place_holder_for_Detailed_desc
	 * @param
	 * @return
	 ***************************************************************/
	void CtlEngineTest(ctl_device_adapter_handle_t hDAhandle)
	{
		PRINT_LOGS("\n::::::::::::::Print Engine Properties::::::::::::::\n");

		uint32_t EngineHandlerCount = 0;
		ctl_result_t res = ctlEnumEngineGroups(hDAhandle, &EngineHandlerCount, nullptr);
		if ((res != CTL_RESULT_SUCCESS) || EngineHandlerCount == 0)
		{
			PRINT_LOGS("\nEngine component not supported. Error: %s", DecodeRetCode(res).c_str());
			return;
		}
		else
		{
			PRINT_LOGS("\nNumber of Engine Handles [%d]", EngineHandlerCount);
		}

		ctl_engine_handle_t* pEngineHandle = new ctl_engine_handle_t[EngineHandlerCount];

		res = ctlEnumEngineGroups(hDAhandle, &EngineHandlerCount, pEngineHandle);

		if (res != CTL_RESULT_SUCCESS)
		{
			PRINT_LOGS("\nError: %s for Engine handle.", DecodeRetCode(res).c_str());
			goto cleanUp;
		}

		for (uint32_t i = 0; i < EngineHandlerCount; i++)
		{
			PRINT_LOGS("\n\nFor Engine Handle [%d]", i);
			ctl_engine_properties_t engineProperties = { 0 };
			engineProperties.Size = sizeof(ctl_engine_properties_t);
			res = ctlEngineGetProperties(pEngineHandle[i], &engineProperties);
			if (res)
			{
				PRINT_LOGS("\nError: %s from Engine get properties.", DecodeRetCode(res).c_str());
			}
			else
			{
				PRINT_LOGS("\n[Engine] Engine type [%s]", ((engineProperties.type == CTL_ENGINE_GROUP_GT) ? "Gt" :
					(engineProperties.type == CTL_ENGINE_GROUP_RENDER) ? "Render" :
					(engineProperties.type == CTL_ENGINE_GROUP_MEDIA) ? "Media" :
					"Unknown"));
			}

			ctl_engine_stats_t engineStats = { 0 };
			engineStats.Size = sizeof(ctl_engine_stats_t);
			const int totalIterations = 3;
			int32_t iterations = totalIterations;
			uint64_t prevActiveCounter = 0, prevTimeStamp = 0;
			do
			{
				res = ctlEngineGetActivity(pEngineHandle[i], &engineStats);

				if (res != CTL_RESULT_SUCCESS)
				{
					PRINT_LOGS("\nError: %s  from Engine get activity.", DecodeRetCode(res).c_str());
				}
				else
				{
					if (totalIterations != iterations) // need previous value
					{
						uint64_t activeDiff = engineStats.activeTime - prevActiveCounter;
						uint64_t timeWindow = engineStats.timestamp - prevTimeStamp;

						double percentActivity = static_cast<double>(activeDiff) / static_cast<double>(timeWindow);
						percentActivity *= 100.0;

						PRINT_LOGS("\n[Engine] Active Time [%I64u]\n", activeDiff);
						PRINT_LOGS("[Engine] Time Stamp [%I64u]\n", timeWindow);
						PRINT_LOGS("[Engine] Usage [%f] \n \n \n", percentActivity);
					}
					prevActiveCounter = engineStats.activeTime;
					prevTimeStamp = engineStats.timestamp;
				}

				iterations--;
				Sleep(200);
			} while (iterations > 0);
		}

	cleanUp:
		delete[] pEngineHandle;
		pEngineHandle = nullptr;
	}

	const char* printType(ctl_data_type_t Type)
	{
		switch (Type)
		{
		case ctl_data_type_t::CTL_DATA_TYPE_INT8:
		{
			return "CTL_DATA_TYPE_INT8";
		}
		break;
		case ctl_data_type_t::CTL_DATA_TYPE_UINT8:
		{
			return "CTL_DATA_TYPE_UINT8";
		}
		break;
		case ctl_data_type_t::CTL_DATA_TYPE_INT16:
		{
			return "CTL_DATA_TYPE_INT16";
		}
		break;
		case ctl_data_type_t::CTL_DATA_TYPE_UINT16:
		{
			return "CTL_DATA_TYPE_UINT16";
		}
		break;
		case ctl_data_type_t::CTL_DATA_TYPE_INT32:
		{
			return "CTL_DATA_TYPE_INT32";
		}
		break;
		case ctl_data_type_t::CTL_DATA_TYPE_UINT32:
		{
			return "CTL_DATA_TYPE_UINT32";
		}
		break;
		case ctl_data_type_t::CTL_DATA_TYPE_INT64:
		{
			return "CTL_DATA_TYPE_INT64";
		}
		break;
		case ctl_data_type_t::CTL_DATA_TYPE_UINT64:
		{
			return "CTL_DATA_TYPE_UINT64";
		}
		break;
		case ctl_data_type_t::CTL_DATA_TYPE_FLOAT:
		{
			return "CTL_DATA_TYPE_FLOAT";
		}
		break;
		case ctl_data_type_t::CTL_DATA_TYPE_DOUBLE:
		{
			return "CTL_DATA_TYPE_DOUBLE";
		}
		break;
		case ctl_data_type_t::CTL_DATA_TYPE_STRING_ASCII:
		{
			return "CTL_DATA_TYPE_STRING_ASCII";
		}
		break;
		case ctl_data_type_t::CTL_DATA_TYPE_STRING_UTF16:
		{
			return "CTL_DATA_TYPE_STRING_UTF16";
		}
		break;
		case ctl_data_type_t::CTL_DATA_TYPE_STRING_UTF132:
		{
			return "CTL_DATA_TYPE_STRING_UTF132";
		}
		break;
		default:
			return "Unknown units";
		}
	}

	const char* printUnits(ctl_units_t Units)
	{
		switch (Units)
		{
		case ctl_units_t::CTL_UNITS_FREQUENCY_MHZ:
		{
			return "Frequency in MHz";
		}
		break;
		case ctl_units_t::CTL_UNITS_OPERATIONS_GTS:
		{
			return "GigaOperations per Second";
		}
		break;
		case ctl_units_t::CTL_UNITS_OPERATIONS_MTS:
		{
			return "MegaOperations per Second";
		}
		break;
		case ctl_units_t::CTL_UNITS_VOLTAGE_VOLTS:
		{
			return "Voltage in Volts";
		}
		break;
		case ctl_units_t::CTL_UNITS_POWER_WATTS:
		{
			return "Power in Watts";
		}
		break;
		case ctl_units_t::CTL_UNITS_TEMPERATURE_CELSIUS:
		{
			return "Temperature in Celsius";
		}
		break;
		case ctl_units_t::CTL_UNITS_ENERGY_JOULES:
		{
			return "Energy in Joules";
		}
		break;
		case ctl_units_t::CTL_UNITS_TIME_SECONDS:
		{
			return "Time in Seconds";
		}
		break;
		case ctl_units_t::CTL_UNITS_MEMORY_BYTES:
		{
			return "Memory in Bytes";
		}
		break;
		case ctl_units_t::CTL_UNITS_ANGULAR_SPEED_RPM:
		{
			return "Angular Speed in RPM";
		}
		break;
		default:
			return "Unknown units";
		}
		return "Unknown units";
	}

	void CtlPowerTelemetryTest(ctl_device_adapter_handle_t hDAhandle)
	{
		PRINT_LOGS("\n:: :: :: :: :: :: ::Print Telemetry:: :: :: :: :: :: ::\n");

		ctl_power_telemetry_t pPowerTelemetry = {};
		pPowerTelemetry.Size = sizeof(ctl_power_telemetry_t);
		ctl_result_t status = ctlPowerTelemetryGet(hDAhandle, &pPowerTelemetry);

		if (status == ctl_result_t::CTL_RESULT_SUCCESS)
		{
			PRINT_LOGS("\nTelemetry Success \n");

			PRINT_LOGS("\nTimeStamp:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.timeStamp.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.timeStamp.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.timeStamp.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.timeStamp.value.datadouble);

			PRINT_LOGS("\nGpu Energy Counter:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.gpuEnergyCounter.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.gpuEnergyCounter.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.gpuEnergyCounter.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.gpuEnergyCounter.value.datadouble);

			PRINT_LOGS("\nGpu Voltage:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.gpuVoltage.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.gpuVoltage.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.gpuVoltage.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.gpuVoltage.value.datadouble);

			PRINT_LOGS("\nGpu Current Frequency:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.gpuCurrentClockFrequency.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.gpuCurrentClockFrequency.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.gpuCurrentClockFrequency.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.gpuCurrentClockFrequency.value.datadouble);

			PRINT_LOGS("\nGpu Current Temperature:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.gpuCurrentTemperature.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.gpuCurrentTemperature.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.gpuCurrentTemperature.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.gpuCurrentTemperature.value.datadouble);

			PRINT_LOGS("\nGpu Activity Counter:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.globalActivityCounter.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.globalActivityCounter.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.globalActivityCounter.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.globalActivityCounter.value.datadouble);

			PRINT_LOGS("\nRender Activity Counter:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.renderComputeActivityCounter.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.renderComputeActivityCounter.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.renderComputeActivityCounter.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.renderComputeActivityCounter.value.datadouble);

			PRINT_LOGS("\nMedia Activity Counter:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.mediaActivityCounter.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.mediaActivityCounter.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.mediaActivityCounter.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.mediaActivityCounter.value.datadouble);

			PRINT_LOGS("\nVRAM Energy Counter:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.vramEnergyCounter.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.vramEnergyCounter.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.vramEnergyCounter.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.vramEnergyCounter.value.datadouble);

			PRINT_LOGS("\nVRAM Voltage:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.vramVoltage.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.vramVoltage.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.vramVoltage.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.vramVoltage.value.datadouble);

			PRINT_LOGS("\nVRAM Frequency:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.vramCurrentClockFrequency.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.vramCurrentClockFrequency.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.vramCurrentClockFrequency.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.vramCurrentClockFrequency.value.datadouble);

			PRINT_LOGS("\nVRAM Effective Frequency:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.vramCurrentEffectiveFrequency.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.vramCurrentEffectiveFrequency.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.vramCurrentEffectiveFrequency.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.vramCurrentEffectiveFrequency.value.datadouble);

			PRINT_LOGS("\nVRAM Read Bandwidth:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.vramReadBandwidthCounter.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.vramReadBandwidthCounter.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.vramReadBandwidthCounter.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.vramReadBandwidthCounter.value.datadouble);

			PRINT_LOGS("\nVRAM Write Bandwidth:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.vramWriteBandwidthCounter.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.vramWriteBandwidthCounter.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.vramWriteBandwidthCounter.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.vramWriteBandwidthCounter.value.datadouble);

			PRINT_LOGS("\nVRAM Temperature:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.vramCurrentTemperature.bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.vramCurrentTemperature.units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.vramCurrentTemperature.type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.vramCurrentTemperature.value.datadouble);

			PRINT_LOGS("\nFan Speed:");
			PRINT_LOGS("\nSupported: %s", ((pPowerTelemetry.fanSpeed[0].bSupported) ? "true" : "false"));
			PRINT_LOGS("\nUnits: %s", printUnits(pPowerTelemetry.fanSpeed[0].units));
			PRINT_LOGS("\nType: %s", printType(pPowerTelemetry.fanSpeed[0].type));
			PRINT_LOGS("\nValue: %f", pPowerTelemetry.fanSpeed[0].value.datadouble);
		}
		else
		{
			PRINT_LOGS("\nError: %s", DecodeRetCode(status).c_str());
		}

		PRINT_LOGS("\n \n");
	}

#endif

void XPUInfo::initIGCL(bool useL0)
{
	ctl_result_t Result = CTL_RESULT_SUCCESS;
	uint32_t Adapter_count = 0;

	ctl_init_args_t CtlInitArgs;
	ctl_api_handle_t hAPIHandle;
	CtlInitArgs.AppVersion = CTL_IMPL_VERSION;
    CtlInitArgs.flags = useL0 ? CTL_INIT_FLAG_USE_LEVEL_ZERO : 0;
	CtlInitArgs.Size = sizeof(CtlInitArgs);
	CtlInitArgs.Version = 0;
	ZeroMemory(&CtlInitArgs.ApplicationUID, sizeof(ctl_application_id_t));
	Result = ctlInit(&CtlInitArgs, &hAPIHandle);

	DebugStream dStr;
	if (CTL_RESULT_SUCCESS == Result)
	{
		// Initialization successful
		// Get the list of Intel Adapters

		Result = ctlEnumerateDevices(hAPIHandle, &Adapter_count, nullptr);
		if (CTL_RESULT_SUCCESS == Result)
		{
			std::vector<ctl_device_adapter_handle_t> hDevices(Adapter_count);

			Result = ctlEnumerateDevices(hAPIHandle, &Adapter_count, hDevices.data());
			if (CTL_RESULT_SUCCESS != Result)
			{
				dStr << ("ctlEnumerateDevices returned failure code: 0x%") << std::hex << Result << std::dec << std::endl;
			}
			else if (0 == Adapter_count)
			{
				dStr << ("IGCL: No adapters found\n");
			}
			else
			{
				for (uint32_t Index = 0; Index < Adapter_count; Index++)
				{
					if (nullptr != hDevices[Index])
					{
						IGCLAdapterPropertiesPtr pStDeviceAdapterProperties(new IGCLAdapterProperties);
						if (nullptr == pStDeviceAdapterProperties->pDeviceID)
						{
							break;
						}

						Result = ctlGetDeviceProperties(hDevices[Index], pStDeviceAdapterProperties.get());

						if (Result != CTL_RESULT_SUCCESS)
						{
							dStr << "ctlGetDeviceProperties returned failure code: 0x" << std::hex << Result << std::dec << std::endl;
							break;
						}

						if (CTL_DEVICE_TYPE_GRAPHICS != pStDeviceAdapterProperties->device_type)
						{
							//printf("This is not a Graphics device \n");
							continue;
						}

						if (0x8086 == pStDeviceAdapterProperties->pci_vendor_id)
						{
							if (nullptr != pStDeviceAdapterProperties->pDeviceID)
							{
								uint64_t AdapterID{ 0 };
								AdapterID = *(static_cast<uint64_t*>(pStDeviceAdapterProperties->pDeviceID));
								//PRINT_LOGS("\nIGCL LUID: 0x%.8zx --> %s \n", AdapterID, pStDeviceAdapterProperties->name);

								auto it = m_Devices.find(AdapterID);
								if (it != m_Devices.end())
								{
									it->second->initIGCLDevice(hDevices[Index], pStDeviceAdapterProperties);
									if (!(m_UsedAPIs & API_TYPE_IGCL))
									{
										m_UsedAPIs |= API_TYPE_IGCL;
										if (useL0)
										{
                                            m_UsedAPIs |= API_TYPE_IGCL_L0;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	else
	{
		dStr << ("IGCL: ctlInit failed!\n");
	}
}

bool CtlMemoryTest_GetMaxBandwidth(ctl_device_adapter_handle_t hDAhandle, I64& outMaxBandwidth)
{
	uint32_t MemoryHandlerCount = 0;
	ctl_result_t res = ctlEnumMemoryModules(hDAhandle, &MemoryHandlerCount, nullptr);
	if ((res != CTL_RESULT_SUCCESS) || MemoryHandlerCount == 0)
	{
		//PRINT_LOGS("\nMemory component not supported. Error: %s", DecodeRetCode(res).c_str());
		return false;
	}
	else
	{
		//PRINT_LOGS("\nNumber of Memory Handles [%d]", MemoryHandlerCount);
	}

	std::vector<ctl_mem_handle_t> pMemoryHandle(MemoryHandlerCount);

	res = ctlEnumMemoryModules(hDAhandle, &MemoryHandlerCount, pMemoryHandle.data());

	if (res == CTL_RESULT_SUCCESS)
	{
		for (uint32_t i = 0; i < MemoryHandlerCount; i++)
		{
			//PRINT_LOGS("\n\nFor Memory Handle [%d]", i);

			//PRINT_LOGS("\n[Memory] Get Memory properties:");

			ctl_mem_properties_t memoryProperties = { 0 };
			memoryProperties.Size = sizeof(ctl_mem_properties_t);
			res = ctlMemoryGetProperties(pMemoryHandle[i], &memoryProperties);

			if (res != CTL_RESULT_SUCCESS)
			{
				//PRINT_LOGS("\nError: %s from Memory get properties.", DecodeRetCode(res).c_str());
			}
			else
			{
				//	PRINT_LOGS("\n[Memory] Bus Width [%d]", memoryProperties.busWidth);
				//	PRINT_LOGS("\n[Memory] Location [%d]", (uint32_t)memoryProperties.location);
				//	PRINT_LOGS("\n[Memory] Number of Channels [%d]", memoryProperties.numChannels);
				//	PRINT_LOGS("\n[Memory] Physical Size [%I64u]", memoryProperties.physicalSize);
				//	PRINT_LOGS("\n[Memory] Memory Type [%d]", memoryProperties.type);

#if 0
				PRINT_LOGS("\n[Memory] Get Memory State:");

				ctl_mem_state_t state = { 0 };
				state.Size = sizeof(ctl_mem_state_t);
				res = ctlMemoryGetState(pMemoryHandle[i], &state);

				if (res != CTL_RESULT_SUCCESS)
				{
					PRINT_LOGS("\nError: %s from Memory get State.", DecodeRetCode(res).c_str());
				}
				else
				{
					PRINT_LOGS("\n[Memory] Memory Size [%I64u]", state.size);
					PRINT_LOGS("\n[Memory] Memory Free [%I64u]", state.free);
				}
#endif
				if (memoryProperties.location == CTL_MEM_LOC_DEVICE)
				{
					//PRINT_LOGS("\n[Memory] Get Memory Bandwidth:");

					ctl_mem_bandwidth_t bandwidth;
					memset(&bandwidth, 0, sizeof(bandwidth));
					bandwidth.Size = sizeof(ctl_mem_bandwidth_t);
					bandwidth.Version = 1;
					res = ctlMemoryGetBandwidth(pMemoryHandle[i], &bandwidth);

					if (res != CTL_RESULT_SUCCESS)
					{
						//PRINT_LOGS("Error: %s from Memory get Bandwidth.", DecodeRetCode(res).c_str());
					}
					else
					{
						if (bandwidth.maxBandwidth)
						{
							//PRINT_LOGS("\tMax Memory Bandwidth = %.2lf GB/s\n", bandwidth.maxBandwidth / double(1024 * 1024 * 1024));
							//PRINT_LOGS("\n[Memory] Time Stamp [%I64u] \n \n", bandwidth.timestamp);
							outMaxBandwidth = bandwidth.maxBandwidth;
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

void Device::initIGCLDevice(ctl_device_adapter_handle_t inHandle, IGCLAdapterPropertiesPtr& inPropsPtr)
{
	{
		DebugStream dStr(!!RUN_IGCL_TELEMETRY_TESTS);
		dStr << "Initializing IGCL device: " << inPropsPtr->name << std::endl;
	}
	m_IGCLAdapterName = inPropsPtr->name;

	if (inPropsPtr->device_type == CTL_DEVICE_TYPE_GRAPHICS)
	{
		updateIfDstVal(m_type, DEVICE_TYPE_UNKNOWN, DEVICE_TYPE_GPU);
	}
	// With older drivers, this is wrong!
	updateIfDstVal(m_props.UMA, UMA_UNKNOWN, 
		(inPropsPtr->graphics_adapter_properties & CTL_ADAPTER_PROPERTIES_FLAG_INTEGRATED) ? UMA_INTEGRATED : NONUMA_DISCRETE);
	m_hIGCLAdapter = inHandle;
	validAPIs = validAPIs | API_TYPE_IGCL;

	IGCLPciPropertiesPtr pIGCLPciProps(new IGCLPciProperties);
	ctl_result_t Result = ctlPciGetProperties(m_hIGCLAdapter, pIGCLPciProps.get());
	DebugStream dStr(false);
	if (Result != CTL_RESULT_SUCCESS)
	{
		dStr << "ctlPciGetProperties returned failure code: 0x" << std::hex << Result << std::dec << std::endl;
	}
	else
	{
		updateIfNotZero(m_props.PCIDeviceGen, pIGCLPciProps->maxSpeed.gen);
		updateIfNotZero(m_props.PCIDeviceWidth, pIGCLPciProps->maxSpeed.width);
		updateIfNotZero(m_props.PCIDeviceMaxBandwidth, pIGCLPciProps->maxSpeed.maxBandwidth);

		Result = ctlPciGetState(m_hIGCLAdapter, &pIGCLPciProps->InitialPCIState);
		if (Result == CTL_RESULT_SUCCESS)
		{
			// Sanity-check IGCL result as some drivers mis-report on certain iGfx parts
			// Current PCIe gen is 4, max width is 16.  Limit to 8 and 64
			if (pIGCLPciProps->InitialPCIState.speed.gen != -1)
			{
				if ((pIGCLPciProps->InitialPCIState.speed.gen > 0) &&
					(pIGCLPciProps->InitialPCIState.speed.gen <= 8))
				{
					updateIfNotZero(m_props.PCICurrentGen, pIGCLPciProps->InitialPCIState.speed.gen);
				}
				else
				{
					dStr << "Invalid data from IGCL: pIGCLPciProps->InitialPCIState.speed.gen = " << pIGCLPciProps->InitialPCIState.speed.gen << std::endl;
				}
			}

			if (pIGCLPciProps->InitialPCIState.speed.width != -1)
			{
				if ((pIGCLPciProps->InitialPCIState.speed.width > 0) &&
					(pIGCLPciProps->InitialPCIState.speed.width <= 64))
				{
					updateIfNotZero(m_props.PCICurrentWidth, pIGCLPciProps->InitialPCIState.speed.width);
				}
				else
				{
					dStr << "Invalid data from IGCL: pIGCLPciProps->InitialPCIState.speed.width = " << pIGCLPciProps->InitialPCIState.speed.width << std::endl;
				}
			}

			double bwDev = m_props.PCIDeviceGen * m_props.PCIDeviceWidth;
			double bwScale = (bwDev > 0.) ? ((m_props.PCICurrentGen * m_props.PCICurrentWidth) / bwDev) : 0.;
			if ((bwScale > 0.) && (m_props.PCIDeviceMaxBandwidth > 0))
			{
				m_props.PCICurrentMaxBandwidth = I64(bwScale * m_props.PCIDeviceMaxBandwidth);
			}
		}

		// For iGfx, getting strange result !supported but enabled - filter out
		if (!m_props.PCIReBAR.valid)
		{
			m_props.PCIReBAR.valid = pIGCLPciProps->resizable_bar_supported ||
				(!pIGCLPciProps->resizable_bar_supported && !pIGCLPciProps->resizable_bar_enabled);
			m_props.PCIReBAR.supported = pIGCLPciProps->resizable_bar_supported;
			m_props.PCIReBAR.enabled = pIGCLPciProps->resizable_bar_enabled;
		}

		if (!m_props.PCIAddress.valid() && isValidPCIAddr(pIGCLPciProps->address))
		{
			m_props.PCIAddress.domain = pIGCLPciProps->address.domain;
			m_props.PCIAddress.bus = pIGCLPciProps->address.bus;
			m_props.PCIAddress.device = pIGCLPciProps->address.device;
			m_props.PCIAddress.function = pIGCLPciProps->address.function;
		}

		if (m_props.MemoryBandWidthMax == -1)
		{
			CtlMemoryTest_GetMaxBandwidth(m_hIGCLAdapter, m_props.MemoryBandWidthMax);
		}
	}

#if RUN_IGCL_TELEMETRY_TESTS
	//CtlFrequencyTest(m_hIGCLAdapter); // Monitor via freqState.actual
	CtlPowerTest(m_hIGCLAdapter); // Peak DC limit not working
	//CtlEngineTest(m_hIGCLAdapter); // Monitor engine stats
	//CtlPowerTelemetryTest(m_hIGCLAdapter); // try vramReadBandwidthCounter, vramWriteBandwidthCounter
#endif
}

#ifdef XPUINFO_USE_TELEMETRYTRACKER
void TelemetryTracker::InitIGCL()
{
	auto hIGCL = m_Device->getHandle_IGCL();
	UI32 FrequencyHandlerCount = 0;
	ctl_result_t res = ctlEnumFrequencyDomains(hIGCL, &FrequencyHandlerCount, nullptr);
	XPUINFO_REQUIRE(res == CTL_RESULT_SUCCESS);

	std::vector<ctl_freq_handle_t> freqHandles(FrequencyHandlerCount);
	res = ctlEnumFrequencyDomains(hIGCL, &FrequencyHandlerCount, freqHandles.data());
	XPUINFO_REQUIRE(res == CTL_RESULT_SUCCESS);

	for (UI32 i=0; i < FrequencyHandlerCount; ++i)
	{
		ctl_freq_properties_t freqProperties = { 0 };
		freqProperties.Size = sizeof(ctl_freq_properties_t);
		res = ctlFrequencyGetProperties(freqHandles[i], &freqProperties);
		if (freqProperties.type == CTL_FREQ_DOMAIN_MEMORY)
		{
			m_IGCL_MemFreqHandle = freqHandles[i];
			m_ResultMask = (TelemetryItem)(m_ResultMask | TELEMETRYITEM_FREQUENCY_MEMORY);
		}
	}
}

bool TelemetryTracker::RecordIGCL(TimedRecord& rec)
{
	auto hIGCL = m_Device->getHandle_IGCL();
	bool bUpdate = false;

	ctl_power_telemetry_t pPowerTelemetry = {};
	pPowerTelemetry.Size = sizeof(ctl_power_telemetry_t);
	ctl_result_t status = ctlPowerTelemetryGet(hIGCL, &pPowerTelemetry);
	TelemetryItem resultMask = TelemetryItem(m_ResultMask | TELEMETRYITEM_TIMESTAMP_DOUBLE);

	if (status == ctl_result_t::CTL_RESULT_SUCCESS)
	{

		XPUINFO_DEBUG_REQUIRE(pPowerTelemetry.timeStamp.bSupported);
		XPUINFO_DEBUG_REQUIRE(CTL_UNITS_TIME_SECONDS == pPowerTelemetry.timeStamp.units);
		XPUINFO_DEBUG_REQUIRE(CTL_DATA_TYPE_DOUBLE == pPowerTelemetry.timeStamp.type);
		rec.timeStamp = pPowerTelemetry.timeStamp.value.datadouble;
		bUpdate = true;

		if (pPowerTelemetry.gpuCurrentClockFrequency.bSupported)
		{
			rec.freq = pPowerTelemetry.gpuCurrentClockFrequency.value.datadouble;
			resultMask = (TelemetryItem)(resultMask | TELEMETRYITEM_FREQUENCY);
		}

		if (pPowerTelemetry.vramReadBandwidthCounter.bSupported)
		{
			XPUINFO_DEBUG_REQUIRE(pPowerTelemetry.vramWriteBandwidthCounter.bSupported);
			XPUINFO_DEBUG_REQUIRE(CTL_DATA_TYPE_UINT64 == pPowerTelemetry.vramReadBandwidthCounter.type);
			rec.bw_read = pPowerTelemetry.vramReadBandwidthCounter.value.datau64;
			rec.bw_write = pPowerTelemetry.vramWriteBandwidthCounter.value.datau64;
			resultMask = (TelemetryItem)(resultMask | TELEMETRYITEM_READ_BW | TELEMETRYITEM_WRITE_BW);
		}


		if (pPowerTelemetry.globalActivityCounter.bSupported)
		{
			XPUINFO_DEBUG_REQUIRE(CTL_UNITS_TIME_SECONDS == pPowerTelemetry.globalActivityCounter.units);
			XPUINFO_DEBUG_REQUIRE(CTL_DATA_TYPE_DOUBLE == pPowerTelemetry.globalActivityCounter.type);
			rec.activity_global = pPowerTelemetry.globalActivityCounter.value.datadouble;
			resultMask = (TelemetryItem)(resultMask | TELEMETRYITEM_GLOBAL_ACTIVITY);
		}

		if (pPowerTelemetry.renderComputeActivityCounter.bSupported)
		{
			XPUINFO_DEBUG_REQUIRE(CTL_UNITS_TIME_SECONDS == pPowerTelemetry.renderComputeActivityCounter.units);
			XPUINFO_DEBUG_REQUIRE(CTL_DATA_TYPE_DOUBLE == pPowerTelemetry.renderComputeActivityCounter.type);
			rec.activity_compute = pPowerTelemetry.renderComputeActivityCounter.value.datadouble;
			resultMask = (TelemetryItem)(resultMask | TELEMETRYITEM_RENDER_COMPUTE_ACTIVITY);
		}

		if (pPowerTelemetry.mediaActivityCounter.bSupported)
		{
			XPUINFO_DEBUG_REQUIRE(CTL_UNITS_TIME_SECONDS == pPowerTelemetry.mediaActivityCounter.units);
			XPUINFO_DEBUG_REQUIRE(CTL_DATA_TYPE_DOUBLE == pPowerTelemetry.mediaActivityCounter.type);
			rec.activity_media = pPowerTelemetry.mediaActivityCounter.value.datadouble;
			resultMask = (TelemetryItem)(resultMask | TELEMETRYITEM_MEDIA_ACTIVITY);
		}

		if (m_IGCL_MemFreqHandle)
		{
			ctl_freq_state_t freqState = { 0 };
			freqState.Size = sizeof(ctl_freq_state_t);
			auto res = ctlFrequencyGetState(m_IGCL_MemFreqHandle, &freqState);
			if (res == CTL_RESULT_SUCCESS)
			{
				rec.freq_memory = freqState.actual;
			}
		}

		if (m_records.size() == 0)
		{
			m_ResultMask = resultMask;
			m_startTime = rec.timeStamp;
		}
	}
	return bUpdate;
}
#endif // XPUINFO_USE_TELEMETRYTRACKER

} // XI
#endif // XPUINFO_USE_IGCL
