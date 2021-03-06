﻿using System;
using System.Collections.Generic;
using System.Text;
using CommonSupport;

namespace CommonFinancial
{
    /// <summary>
    /// Provides a base class for any order class. Holds only the most common order parameters.
    /// Currently inherited by ActiveOrder and MasterOrder classes.
    /// </summary>
    [Serializable]
    public abstract class Order : IDisposable
    {
        /// <summary>
        /// Different modes of order result, used in GetResult().
        /// </summary>
        public enum ResultModeEnum
        {
            Currency,
            Raw,
            Pips, // Pips do not consider closeVolume.
            AccountBaseCurrency
        }

        /// <summary>
        /// When order is updated, this shows the type of update it has undergone.
        /// </summary>
        public enum UpdateTypeEnum
        {
            Submitted,
            Executed, // ActiveOrder is actually opened (usefull for delayed orders, to note when they activate).
            VolumeChanged, // Volume changed.
            Closed, // ActiveOrder closed.
            Canceled,
            Modified, // ActiveOrder parameters modified.
            Update,
            CriticalModified // A critical existing parameter of the order was modified (for ex. open/close price)
            //Terminated // ActiveOrder has been terminated from existence (may be done by order itself to evade lost of synchronization)
        }

        #region Member Variables

        protected OrderInfo _info;
        /// <summary>
        /// Core order information.
        /// </summary>
        public virtual OrderInfo Info
        {
            get { return _info; }
        }

        /// <summary>
        /// Id of this order; unique for each order executing source, but not unique for the whole system.
        /// For whole system, Guid can be used.
        /// </summary>
        public virtual string Id
        {
            get { return _info.Id; }
        }

        /// <summary>
        /// Tag usually stores extra information required for the identification of the order.
        /// </summary>
        public virtual string Tag
        {
            get { return _info.Tag; }
        }

        /// <summary>
        /// ActiveOrder type, see OrderTypeEnum for details.
        /// </summary>
        public virtual OrderTypeEnum Type
        {
            get
            {
                if (_info.Type == OrderTypeEnum.UNKNOWN)
                {
                    SystemMonitor.OperationWarning("Order type not established yet.");
                }

                return _info.Type;
            }
        }

        /// <summary>
        /// The current state of the order.
        /// </summary>
        public virtual OrderStateEnum State
        {
            get { return _info.State; }
            set { _info.State = value; }
        }

        /// <summary>
        /// ActiveOrder is opened or is (delayed) pending.
        /// </summary>
        public virtual bool IsOpenOrPending
        {
            get { return State == OrderStateEnum.Executed || State == OrderStateEnum.Submitted; }
        }

        /// <summary>
        /// Stop loss at the executing platform (may be an external platform).
        /// </summary>
        public virtual Decimal? StopLoss
        {
            get { lock (this) { return _info.StopLoss; } }
        }

        /// <summary>
        /// Open time at the executing platform (may be an external platform).
        /// </summary>
        public virtual DateTime? OpenTime
        {
            get { lock (this) { return _info.OpenTime; } }
        }

        /// <summary>
        /// Close time at the executing platform (may be an external platform).
        /// </summary>
        public virtual DateTime? CloseTime
        {
            get { lock (this) { return _info.CloseTime; } }
        }

        /// <summary>
        /// Has stop loss been assigned for this order.
        /// </summary>
        public virtual bool StopLossAssigned
        {
            get
            {
                lock (this)
                {
                    return (_info.StopLoss.HasValue
                        && _info.StopLoss.Value != 0);
                }
            }
        }

        /// <summary>
        /// Take profit at the executing platform (may be an external platform).
        /// </summary>
        public virtual Decimal? TakeProfit
        {
            get { lock (this) { return _info.TakeProfit; } }
        }

        /// <summary>
        /// Has take profit been assigned for this order.
        /// </summary>
        public virtual bool TakeProfitAssigned
        {
            get
            {
                lock (this)
                {
                    return (_info.TakeProfit.HasValue && _info.TakeProfit.Value != 0);
                }
            }
        }

        /// <summary>
        /// Current order closeVolume (can be modified once the order is opened).
        /// </summary>
        public virtual int CurrentVolume
        {
            get { return _info.Volume; }
        }

        /// <summary>
        /// Current order closeVolume (can be modified once the order is opened).
        /// </summary>
        public virtual int CurrentDirectionalVolume
        {
            get 
            {
                if (IsBuy)
                {
                    return _info.Volume;
                }
                else
                {
                    return -_info.Volume;
                }
            }
        }

        /// <summary>
        /// ActiveOrder open price (applicable for opened and pending orders only).
        /// If the order is pending, this will be the desired target opening price.
        /// </summary>
        public virtual Decimal? OpenPrice
        {
            get
            {
                lock (this)
                {
                    return _info.OpenPrice;
                }
            }
        }

        /// <summary>
        /// ActiveOrder close price (applicable for closed orders only).
        /// </summary>
        public virtual Decimal? ClosePrice
        {
            get
            {
                lock (this)
                {
                    return _info.ClosePrice;
                }
            }
        }

        protected int _initialVolume = 0;
        /// <summary>
        /// Initial closeVolume corresponds to the closeVolume that the order had on opening.
        /// </summary>
        public virtual int InitialVolume
        {
            get { return _initialVolume; }
        }

        /// <summary>
        /// Initial closeVolume, directional (positive for buy orders, negative for sell orders).
        /// Initial closeVolume corresponds to the closeVolume that the order had on opening.
        /// </summary>
        public virtual Decimal InitialDirectionalVolume
        {
            get
            {
                if (IsBuy)
                {
                    return InitialVolume;
                }
                else
                {
                    return -InitialVolume;
                }
            }
        }

        /// <summary>
        /// Is a Buy order.
        /// </summary>
        public virtual bool IsBuy
        {
            get
            {
                return _info.IsBuy;
            }
        }

        /// <summary>
        /// Is a sell order.
        /// </summary>
        public virtual bool IsSell
        {
            get
            {
                return _info.IsSell;
            }
        }

        /// <summary>
        /// Is this a delayed type of order. Delayed orders are not executed immediately,
        /// but on achieving a certain condition (price, time etc.)
        /// </summary>
        public virtual bool IsDelayed
        {
            get
            {
                return _info.IsDelayed;
            }
        }

        /// <summary>
        /// The id baseCurrency this order is executed against.
        /// </summary>
        public virtual Symbol Symbol 
        {
            get
            {
                return _info.Symbol;
            }
        }

        #endregion

        /// <summary>
        /// 
        /// </summary>
        public delegate void OrderUpdatedDelegate(Order order, UpdateTypeEnum updateType);

        [field: NonSerialized]
        public event OrderUpdatedDelegate OrderUpdatedEvent;


        #region Instance Control

        /// <summary>
        /// Constructor.
        /// </summary>
        public Order()
        {
        }

        /// <summary>
        /// Will create the corresponding order, based to the passed in order information.
        /// Used to create corresponding orders to ones already existing in the platform.
        /// </summary>
        public virtual bool AdoptInfo(OrderInfo info)
        {
            SystemMonitor.CheckError(string.IsNullOrEmpty(this.Id) || info.Id == this.Id, "Order Id changed, this is not expected behaviour.");
            lock (this)
            {
                _info = info;
                _initialVolume = 1;
            }

            return true;
        }

        /// <summary>
        /// 
        /// </summary>
        public virtual bool UpdateInfo(OrderInfo info)
        {
            lock (this)
            {
                return _info.Update(info);
            }
        }

        /// <summary>
        /// 
        /// </summary>
        public virtual void Dispose()
        {
        }

        #endregion

        #region Implementation

        protected void RaiseOrderUpdatedEvent(UpdateTypeEnum updateType)
        {
            if (OrderUpdatedEvent != null)
            {
                OrderUpdatedEvent(this, updateType);
            }
        }

        /// <summary>
        /// 
        /// </summary>
        public abstract Decimal? GetResult(ResultModeEnum mode);

        /// <summary>
        /// Called from the order execution provider to notify this pending order has been executed.
        /// </summary>
        /// <param name="openPrice"></param>
        public virtual void AcceptPendingExecuted(decimal openPrice)
        {
            if (State != OrderStateEnum.Submitted)
            {
                SystemMonitor.OperationError("Order not pending.");
            }

            lock (this)
            {
                _info.OpenPrice = openPrice;
            }

            State = OrderStateEnum.Executed;
        }

        /// <summary>
        /// Put out some text information on this object.
        /// </summary>
        /// <param name="fullPrint">Show full or partial information only.</param>
        /// <returns></returns>
        public virtual string Print(bool fullPrint)
        {
            if (Symbol != Symbol.Emtpy)
            {
                return string.Format("Symbol {0} Type {1}, Open {2} ", Symbol.Name, Type.ToString(), OpenPrice.ToString());
            }
            else
            {
                return "Order not initialized.";
            }
        }

        #endregion

        #region Static

        public static decimal? GetResult(ResultModeEnum mode, decimal? open, decimal? close, decimal volume, Symbol orderSymbol,
            OrderStateEnum state, OrderTypeEnum type, CurrencyConversionManager convertor, Symbol accountCurrency, 
            decimal lotSize, int decimalPlaces, decimal? ask, decimal? bid)
        {
            if (/*ask.HasValue == false || bid.HasValue == false */
                string.IsNullOrEmpty(orderSymbol.Name))
            {
                return null;
            }

            Decimal? currentRawResult = null;
            if (state == OrderStateEnum.Executed)
            {
                // Update result.
                currentRawResult = GetRawResult(open, volume, state, type, ask, bid, null, mode != ResultModeEnum.Pips);
            }
            else if (state == OrderStateEnum.Closed)
            {
                currentRawResult = GetRawResult(open, volume, state, type, null, null, close.Value, mode != ResultModeEnum.Pips);
            }

            if (currentRawResult.HasValue == false)
            {
                return null;
            }

            if (mode == ResultModeEnum.Pips)
            {
                //if (state == OrderStateEnum.Closed)
                //{// When closed we need to compensate the 
                //    if (OrderInfo.TypeIsBuy(type))
                //    {
                //        return (close - open) * (decimal)Math.Pow(10, decimalPlaces);
                //    }
                //    else
                //    {
                //        return (open - close) * (decimal)Math.Pow(10, decimalPlaces);
                //    }
                //}
                //else
                //{
                    return currentRawResult * (decimal)Math.Pow(10, decimalPlaces);
                //}
            }
            else if (mode == ResultModeEnum.Raw)
            {
                return currentRawResult;
            }
            else if (mode == ResultModeEnum.Currency)
            {
                return currentRawResult;
            }
            else if (mode == ResultModeEnum.AccountBaseCurrency)
            {
                if (string.IsNullOrEmpty(accountCurrency.Name) || convertor == null)
                {
                    SystemMonitor.Warning("Mode requires the Account Currency and Convertion Manager to be specified.");
                    return null;
                }

                if (orderSymbol.IsForexPair)
                {// We have a forex pair and need to rebase to account base currency.
                    double? conversionRate = convertor.GetRate(orderSymbol.ForexCurrency2, accountCurrency.Name, TimeSpan.FromSeconds(1.5), true);
                    if (conversionRate.HasValue == false)
                    {
                        SystemMonitor.OperationError("Failed to establish conversion rate between [" + orderSymbol.ForexCurrency2 + "] and [" + accountCurrency.Name + "].");
                        return null;
                    }
                }
                else
                {// All other symbols are by default in account base currency prices.
                    return currentRawResult.Value;
                }
            }

            SystemMonitor.NotImplementedCritical("Mode not supported.");
            return 0;
        }

        /// <summary>
        /// 
        /// </summary>
        public static Decimal? GetRawResult(decimal? open, decimal volume, OrderStateEnum state,
            OrderTypeEnum type, decimal? ask, decimal? bid, decimal? close, bool considerVolume)
        {
            if (state != OrderStateEnum.Closed && state != OrderStateEnum.Executed)
            {
                if (state == OrderStateEnum.Failed || state == OrderStateEnum.Initialized
                    || state == OrderStateEnum.UnInitialized || state == OrderStateEnum.Unknown)
                {
                    return null;
                }
                
                // Canceled, Submitted
                return 0;
            }

            if (open.HasValue == false 
                || (state == OrderStateEnum.Executed && (ask.HasValue == false || bid.HasValue == false))
                || (state == OrderStateEnum.Closed && close.HasValue == false))
            {
                return null;
            }

            decimal currentValue = 0;

            if (state == OrderStateEnum.Executed)
            {
                currentValue = OrderInfo.TypeIsBuy(type) ? bid.Value : ask.Value;
            }
            else if (state == OrderStateEnum.Closed)
            {
                currentValue = close.Value;
            }
            else
            {
                return null;
            }

            Decimal difference = 0;
            if (OrderInfo.TypeIsBuy(type))
            {
                difference = currentValue - open.Value;
            }
            else
            {
                difference = open.Value - currentValue;
            }

            if (considerVolume)
            {
                return volume * difference;
            }
            else
            {
                return difference;
            }
        }

        //public override decimal CompensateLotSize(decimal inputValue)
        //{
        //    // Log gives how many times power takes to get from 10 to ask. Can be negative (for ex. ask = 0.5)
        //    if (OrderExecutionProvider.Account.OperationalState == OperationalStateEnum.Operational)
        //    {
        //        if (OrderExecutionProvider.SessionDataProvider.Info.Symbol.Name.StartsWith(OrderExecutionProvider.Account.Info.BaseCurrency.Name))
        //        {// Compensation.
        //            return inputValue /= 100;
        //        }
        //    }
        //    return inputValue;
        //}

        #endregion

    }
}
