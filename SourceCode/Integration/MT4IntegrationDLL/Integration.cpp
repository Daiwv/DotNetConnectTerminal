#include "gachelper.h"

// Include only this, if GacHelper not included.
//#include <windows.h>

// Adding those confuses the Linker and results in errors, so no STL!
//#include <vector>
//#include <string>

#define EXPORTED __declspec(dllexport)

//ALL MT4 EXPORTS NEED TO BE DECLARED IN THE DEF FILE AS WELL !!
#pragma push_macro("new")
#undef new

// Since many experts of the same type will be talking simultaniously to this DLL, it has to have sessioning, thread protection etc.
// The way to distinguish one expert instance from another is trough the ExpertID.

class AdapterMediator
{
	System::Runtime::InteropServices::GCHandle gcHandle;
	HANDLE _mutexHandle;

	bool _initialized;

public:
	AdapterMediator()
	{
		_mutexHandle = CreateMutex(0, FALSE, 0);
		_initialized = false;
	}

	~AdapterMediator()
	{
		MT4Adapter::MT4RemoteAdapter* adapter = GetAdapter();
		if (adapter != NULL)
		{
			adapter->UnInitialize();
		}

		CloseHandle(_mutexHandle);
		_mutexHandle = NULL;
		gcHandle.Free();
		
		// This prevents a crash in the terminal on exit, for some reason.
		System::Threading::Thread::Sleep(5000);
	}

	void InitializeServer(System::String* serverAddress)
	{
		WaitForSingleObject(_mutexHandle, INFINITE);

		if (_initialized)
		{// Already allocated.
			if (System::String::Compare(serverAddress, GetAdapter()->ServerIntegrationUri->ToString()) != 0)
			{
				System::String* message = System::String::Format("Conflicting server addresses in experts on the same MT4 instance found {0} [initial: {1}, passed: {2}].{0}Set them all the same and restart the MT4.", System::Environment::NewLine, GetAdapter()->ServerIntegrationUri->ToString(), serverAddress);
				System::Windows::Forms::MessageBox::Show(message, "MT4 OFxP Expert Error");
			}
			ReleaseMutex(_mutexHandle);
			return;
		}

		_initialized = true;

		System::Diagnostics::Trace::WriteLine("AdapterMediator::InitializeServer [1]");
		if (System::String::IsNullOrEmpty(serverAddress))
		{// Assign the default value.
			System::Windows::Forms::MessageBox::Show("Assigned a server on default address.", "MT4 OFxP Expert Warning");
			serverAddress = "net.tcp://localhost:13123/TradingAPI";
		}

		System::Diagnostics::Trace::WriteLine("AdapterMediator::InitializeServer [2]");
		System::Uri* serverIntegrationUri = new System::Uri(serverAddress);
		MT4Adapter::MT4RemoteAdapter* integrationAdapter = new MT4Adapter::MT4RemoteAdapter(serverIntegrationUri);
		gcHandle = System::Runtime::InteropServices::GCHandle::Alloc(integrationAdapter);

		ReleaseMutex(_mutexHandle);
		System::Diagnostics::Trace::WriteLine("AdapterMediator::InitializeServer [3]");
	}

	MT4Adapter::MT4RemoteAdapter* GetAdapter()
	{
		WaitForSingleObject(_mutexHandle, INFINITE);
		
		MT4Adapter::MT4RemoteAdapter* result = static_cast<MT4Adapter::MT4RemoteAdapter*>(gcHandle.Target);
		
		ReleaseMutex(_mutexHandle);
		return result;
	}

};

AdapterMediator AdapterMediator;

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
void TraceEntry(const char* methodName, const char* parameter)
{
	System::String* methodString = new System::String(methodName);
	System::Diagnostics::Trace::Write(System::String::Format(methodString->Concat(methodString, new System::String("::{0}")), new System::String(parameter)));
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
//void Trace(const char* methodName, const char* message, const char* parameter)
//{
//	System::String* methodString = new System::String(methodName);
//	System::String* messageString = new System::String(message);
//
//	System::Diagnostics::Trace::Write(System::String::Format(methodString->Concat(methodString, new System::String("::{0}")), new System::String(parameter)));
//}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
// Helper, static
const char* MarshalStringToUnmanaged(System::String* inputString)
{
	System::IntPtr strPtr = System::Runtime::InteropServices::Marshal::StringToHGlobalAnsi(inputString);
	return (const char*)strPtr.ToPointer();
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
// Helper, static
void MarshalFreeString(const char* data)
{
	System::IntPtr strPtr((void*)data);
	System::Runtime::InteropServices::Marshal::FreeHGlobal(strPtr);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
EXPORTED int __stdcall InitializeServer(const char* serverAddress)
{// Since there will be multiple calls to this (from multiple experts in same DLL), 
	
	TraceEntry("InitializeServer", serverAddress);
	//System::Diagnostics::Trace:: Write("InitializeServer::");
	//System::Diagnostics::Trace::WriteLine(serverAddress);

	bool queryResult = GACHelper::QueryAssembly("Arbiter");
	queryResult = queryResult && GACHelper::QueryAssembly("MT4Adapter");

	if (queryResult == false)
	{
		System::Windows::Forms::MessageBox::Show("MT4 Initialization failed. Requrired assemblies not found in GAC.", "MT4 OFxP Expert Error");
		return 0;
	}

	// Only the first valid one will be considered.
	AdapterMediator.InitializeServer(serverAddress);

	return 1;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
// Not currently used, could be used as a way to release strings passed back to the MT4 expert in case there is a proven memory leak in those.
EXPORTED void __stdcall FreeString(const char* data)
{
	MarshalFreeString(data);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
// This is called once for each expert.
EXPORTED int __stdcall InitializeIntegration(const char* expertID, const char* symbol,
double modePoint, double modeDigits, double modeSpread, double modeStopLevel, double modeLotSize, double modeTickValue,
double modeTickSize, double modeSwapLong, double modeSwapShort, double modeStarting, double modeExpiration,
double modeTradeAllowed, double modeMinLot, double modeLotStep, double modeMaxLot, double modeSwapType,
double modeProfitCalcMode, double modeMarginCalcMode, double modeMarginInit, double modeMarginMaintenance,
double modeMarginHedged, double modeMarginRequired, double modeFreezeLevel)
{
	TraceEntry("InitializeIntegration", expertID);
	//System::Diagnostics::Trace::Write("InitializeIntegration::");

	return AdapterMediator.GetAdapter()->InitializeIntegrationSession(symbol, 
      modePoint, modeDigits, modeSpread, modeStopLevel, modeLotSize, modeTickValue,
      modeTickSize, modeSwapLong, modeSwapShort, modeStarting, modeExpiration,
      modeTradeAllowed, modeMinLot, modeLotStep, modeMaxLot, modeSwapType,
      modeProfitCalcMode, modeMarginCalcMode, modeMarginInit, modeMarginMaintenance,
      modeMarginHedged, modeMarginRequired, modeFreezeLevel);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
// This is called once for each expert.
EXPORTED int __stdcall AddSymbolPeriod(const char* expertID, const char* symbol, int period)
{
	TraceEntry("AddSymbolPeriod", expertID);

	return AdapterMediator.GetAdapter()->AddSessionPeriod(symbol, period);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
// This is called once for each expert.
EXPORTED int __stdcall UnInitializeIntegration(const char* expertID, const char* symbol)
{
	TraceEntry("UnInitializeIntegration", expertID);
//	AdapterMediator.GetAdapter()->UnInitialize();

	return 1;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
// Reference parameters can not be read or written to at all! We need to use return strings.
//(double& amount...
EXPORTED const char* __stdcall RequestNewOrder(const char* expertID, const char* symbol) 
{
	TraceEntry("RequestNewOrder", expertID);

	return MarshalStringToUnmanaged(AdapterMediator.GetAdapter()->RequestNewOrder());
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
// Reference parameters can not be read or written to at all!  We need to use strings. 
EXPORTED const char* __stdcall RequestOrderInformation(const char* expertID)
{
	TraceEntry("RequestOrderInformation", expertID);
	return MarshalStringToUnmanaged(AdapterMediator.GetAdapter()->RequestOrderInformation());
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
// Reference parameters can not be read or written to at all!  We need to use strings. 
EXPORTED const char* __stdcall RequestCloseOrder(const char* expertID, const char* symbol) //(int& orderTicket, int& operationID ...)
{
	TraceEntry("RequestCloseOrder", expertID);
	return MarshalStringToUnmanaged(AdapterMediator.GetAdapter()->RequestCloseOrder());
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
// Reference parameters can not be read or written to at all!  We need to use strings. 
EXPORTED const char* __stdcall RequestModifyOrder(const char* expertID, const char* symbol) //(int& orderTicket, int& operationID)
{
	TraceEntry("RequestModifyOrder", expertID);
	return MarshalStringToUnmanaged(AdapterMediator.GetAdapter()->RequestModifyOrder());
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
// Reference parameters can not be read or written to at all!  We need to use strings. 
EXPORTED int __stdcall RequestAllOrders(const char* expertID) //(int& operationID > 0)
{
	TraceEntry("RequestAllOrders", expertID);
	//return AdapterMediator.GetAdapter()->RequestAllOrders();
	
	return 0;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
EXPORTED const char* __stdcall RequestValues(const char* expertID) // operationId; int (preffered count)
{
	TraceEntry("RequestValues", expertID);
	return MarshalStringToUnmanaged(AdapterMediator.GetAdapter()->RequestValues(expertID));
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
EXPORTED void __stdcall OrderOpened(const char* expertID, const char* symbol, int operationID, int orderTicket, double openingPrice, int orderOpenTime, int operationResult, const char* operationResultMessage)
{
	TraceEntry("OrderOpened", expertID);
	AdapterMediator.GetAdapter()->OrderOpened(symbol, operationID, orderTicket, openingPrice, orderOpenTime, operationResult != 0, operationResultMessage);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
EXPORTED void __stdcall OrderClosed(const char* expertID, const char* symbol, int operationID, int orderTicket, int orderNewTicket, double closingPrice, int orderCloseTime, int operationResult, const char* operationResultMessage)
{
	TraceEntry("OrderClosed", expertID);
	AdapterMediator.GetAdapter()->OrderClosed(symbol, operationID, orderTicket, 
		orderNewTicket, closingPrice, orderCloseTime, operationResult != 0, operationResultMessage);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
EXPORTED void __stdcall OrderModified(const char* expertID, const char* symbol, int operationID, int orderTicket, int orderNewTicket, int operationResult, const char* operationResultMessage)
{
	TraceEntry("OrderModified", expertID);
	AdapterMediator.GetAdapter()->OrderModified(symbol, operationID, orderTicket, 
		orderNewTicket, operationResult != 0, operationResultMessage);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
EXPORTED void __stdcall AllOrders(const char*  expertID, const char* symbol, int operationID, 
								  int openCount, const int* openCustomIDs, const int* openTickets, 
								  int historicalCount, const int* historicalCustomIDs, const int* historicalTickets, 
								  int operationResult)
{
	TraceEntry("AllOrders", expertID);
	
	// Copy over the data into proper managed arrays.
	System::Int32 openManagedCustomIDs __gc[] = new System::Int32 __gc[openCount];
	System::Int32 openManagedTickets __gc[] = new System::Int32 __gc[openCount];
	
	for(int i = 0;  i < openCount; i++)
	{
		openManagedCustomIDs[i] = openCustomIDs[i];
		openManagedTickets[i] = openTickets[i];		
	}

	// Copy over the data into proper managed arrays.
	System::Int32 historicalManagedCustomIDs __gc[] = new System::Int32 __gc[historicalCount];
	System::Int32 historicalManagedTickets __gc[] = new System::Int32 __gc[historicalCount];
	
	for(int i = 0;  i < historicalCount; i++)
	{
		historicalManagedCustomIDs[i] = historicalCustomIDs[i];
		historicalManagedTickets[i] = historicalTickets[i];		
	}

	AdapterMediator.GetAdapter()->AllOrders(operationID, symbol,
		openManagedCustomIDs, openManagedTickets, 
		historicalManagedCustomIDs, historicalManagedTickets, 
		operationResult != 0);
}


// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
EXPORTED void __stdcall ErrorOccured(const char* expertID, int operationID, const char* errorMessage)
{
	TraceEntry("ErrorOccured", expertID);
	
	AdapterMediator.GetAdapter()->ErrorOccured(operationID, errorMessage);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
EXPORTED void __stdcall OrderInformation(const char* expertID, const char* symbol, int operationID, int orderTicket, const char* orderSymbol, int orderType, double volume, 
										 double openPrice, double closePrice, double orderStopLoss, double orderTakeProfit, 
										 double currentProfit, double orderSwap, int orderPlatformOpenTime, 
										 int orderPlatformCloseTime, int orderExpiration, double orderCommission,
										 const char* orderComment, int orderCustomID, int operationResult, const char* operationResultMessage)
{
	TraceEntry("OrderInformation", expertID);
	
	AdapterMediator.GetAdapter()->OrderInformation(orderTicket, operationID, orderSymbol, orderType, volume, 
										 openPrice, closePrice, orderStopLoss, orderTakeProfit, 
										 currentProfit, orderSwap, orderPlatformOpenTime, 
										 orderPlatformCloseTime, orderExpiration, orderCommission,
										 orderComment, orderCustomID, operationResult != 0, operationResultMessage);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
EXPORTED void __stdcall Quotes(const char* expertId, const char* symbol, int operationId, double ask, double bid, 
								   double open, double close, double low, double high, double volume, double time)
{
	TraceEntry("Quotes", expertId);

	AdapterMediator.GetAdapter()->Quotes(symbol, operationId, ask, bid, open, close, low, high, volume, time);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
EXPORTED void __stdcall TradingValues(const char* expertID, const char* symbol, int operationId, double time, int period,
									  int arrayRatesCount, int requestedValueCount, int availableBarsCount, const double* rates)
{
	TraceEntry("TradingValues", expertID);

	//CommonSupport::TracerHelper::TraceSimple(CommonSupport::TracerItem::ItemTypeEnum::Trace, System::String::Format("TradingValues {0}", requestedValueCount.ToString()));

	// Copy over the data into proper managed arrays.
	System::Int64 timesCopy __gc[] = new System::Int64 __gc[requestedValueCount];
	System::Decimal opensCopy __gc[] = new System::Decimal __gc[requestedValueCount];
	System::Decimal closesCopy __gc[] = new System::Decimal __gc[requestedValueCount];
	System::Decimal highsCopy __gc[] = new System::Decimal __gc[requestedValueCount];
	System::Decimal lowsCopy __gc[] = new System::Decimal __gc[requestedValueCount];
	System::Decimal volumesCopy __gc[] = new System::Decimal __gc[requestedValueCount];

	//int startingIndex = 0;
	//System::Diagnostics::Trace::Write(System::String::Format(System::String::Concat("TradingValues", new System::String("::{0}")), 
	//	requestedValueCount.ToString()));
	//System::Diagnostics::Trace::Write(System::String::Format(System::String::Concat("TradingValues", new System::String("::{0}")), 
	//	arrayRatesCount.ToString()));
	//System::Diagnostics::Trace::Write(System::String::Format(System::String::Concat("TradingValues", new System::String("::{0}")), 
	//	availableBarsCount.ToString()));

	try
	{
		//for	(int i = arrayRatesCount - 1; i >= 0 && i >= arrayRatesCount - requestedValueCount; i--)
		for	(int i = 0; i < requestedValueCount; i++)
		{// There are 6 elements in each set, one after the other.
			// Actually the data in DateTime is kept in 4 bytes (according to docs),
			// but all here seems to be 8B.
			int rateIndex = i + arrayRatesCount - requestedValueCount;
			timesCopy[i] = (System::Int64)rates[rateIndex * 6];
			opensCopy[i] = rates[rateIndex * 6 + 1];
			lowsCopy[i] = rates[rateIndex * 6 + 2];
			highsCopy[i] = rates[rateIndex * 6 + 3];
			closesCopy[i] = rates[rateIndex * 6 + 4];
			volumesCopy[i] = rates[rateIndex * 6 + 5];
		}
	}
	catch(System::Exception* ex)
	{
		System::Diagnostics::Trace::Write(System::String::Format(
			System::String::Concat("TradingValues", new System::String("::{0}")), ex->get_Message()));
	}

	AdapterMediator.GetAdapter()->TradingValuesUpdate(symbol, operationId, time, period, availableBarsCount, 
		timesCopy, opensCopy, closesCopy, highsCopy, lowsCopy, volumesCopy);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 

EXPORTED void __stdcall AccountInformation(const char* expertID, int operationID,
        double accountBalance, double accountCredit, const char* accountCompany,
        const char* accountCurrency, double accountEquity, double accountFreeMargin,
        double accountLeverage, double accountMargin, const char* accountName,
        int accountNumber, double accountProfit, const char* accountServer, 
		int operationResult, const char* operationResultMessage)
{
	TraceEntry("AccountInformation", expertID);

	AdapterMediator.GetAdapter()->AccountInformation(operationID, accountBalance, accountCredit, 
            accountCompany, accountCurrency, 
            accountEquity, accountFreeMargin, accountLeverage, 
            accountMargin, accountName, accountNumber,
            accountProfit, accountServer, operationResult != 0, operationResultMessage);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 

EXPORTED int __stdcall RequestAccountInformation(const char* expertID)
{
	TraceEntry("RequestAccountInformation", expertID);

	return AdapterMediator.GetAdapter()->RequestAccountInformation();
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 


#pragma pop_macro("new")