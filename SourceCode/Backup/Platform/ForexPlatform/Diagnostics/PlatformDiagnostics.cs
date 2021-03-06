﻿using System;
using System.Collections.Generic;
using System.Text;
using CommonSupport;
using System.Runtime.Serialization;

namespace ForexPlatform
{
    /// <summary>
    /// The Diagnostics component in a platform. Allows to view internal operation messages, errors etc.
    /// Good for diagnosting problems, delays, bugs and other issues.
    /// </summary>
    [Serializable]
    [UserFriendlyName("Diagnostics")]
    [ComponentManagement(false, false, 1, false)]
    public class PlatformDiagnostics : PlatformComponent
    {// Being a core platfrom component, make sure it component level is very low level.
        
        /// <summary>
        /// 
        /// </summary>
        public PlatformDiagnostics()
            : base(true)
        {
            this.Name = UserFriendlyNameAttribute.GetTypeAttributeName(typeof(PlatformDiagnostics));
        }

        /// <summary>
        /// Deserialization constructor.
        /// </summary>
        public PlatformDiagnostics(SerializationInfo info, StreamingContext context)
            : base(info, context)
        {
            TracerHelper.Tracer.Enabled = info.GetBoolean("tracerEnabled");
        }

        public override void GetObjectData(SerializationInfo info, StreamingContext context)
        {
            base.GetObjectData(info, context);
            info.AddValue("tracerEnabled", TracerHelper.Tracer.Enabled);
        }

        protected override bool OnInitialize(Platform platform)
        {
            bool result = base.OnInitialize(platform);
            ChangeOperationalState(OperationalStateEnum.Operational);
            return result;
        }

        protected override bool OnUnInitialize()
        {
            ChangeOperationalState(OperationalStateEnum.NotOperational);
            return base.OnUnInitialize();
        }
    }
}
