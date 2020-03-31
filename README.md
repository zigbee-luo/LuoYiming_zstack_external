# LuoYiming_zstack_external

It is based on SDK 3.40 (zstack core 3.6.0), but it is different from original edition.

What is New:

1，In AF.h，new parameter "afCnfCB" and "cnfParam" was added into message "AF_DATA_CONFIRM_MSG". New function "AF_DataRequestExt" has been instead of "AF_DataRequest". This change also effect in ZDP and ZCL and "zstacktask" who will execute "AF_DataRequest".

2, In ZDP and ZCL, "ZDP_SetSendConfirm" and "zcl_SetSendExtParam" can set the "afCnfCB" and "cnfParam" of "AF_DATA_CONFIRM_MSG". Especially in ZDP, "afCnfCB" is executed in Zstack-task, and Application-task who has called ZDP-Command-Request function will receives "zstackmsg_CmdIDs_AF_DATA_CONFIRM_IND" sent by ZDP-layer.

3, In ZCL, manufacturer code was supported. New attribute control bit "ACCESS_MANU_ATTR" can mark a manufacturer-specific attribute. And "CMD_FLAG_MANUCODE" can mark a manufacturer-specific command.

4, BDB_REPORTING supports manufacturer-specific attribute and client-attribute.

5, Correctly Link-Key management. "ZDSecMgrDeviceJoin" is the only access to filter the right joiner. Both directly linking to TC and joining path through Routers, the TC will generate its APS-Link-Key only after "ZDSecMgrDeviceJoin" returns "ZSuccess" . So the "bdbGCB_TCLinkKeyExchangeProcess" is called with "BDB_TC_LK_EXCH_PROCESS_JOINING", the node is first time joining, different from "Rejoin" and "TC-rejoin" . 

And there are many issues in zstack have been fixed by me. There are many modifying that I have no time to describe, I will note them next time.


====================================================================================================================================

How to use "AF-Data-Confirm-Callback":

1, "AF_DataRequestExt" offers 2 new parameter, "afCnfCB" is the callback function that is executed when AF_DATA_CONFIRM_MSG is triggered. And the callback's parameter "cnfParam" is a pointer which is set before AF_DataRequestExt executed and can be processed in callback executed.

2, To send a ZDP command with data-confirm-callback, structure member "zstack_zdoExtParam_t" has been added into every ZDO Interface Request Structures. When your code calls any ZDP-Request function, "extParam" in its input parameter can be filled with callback-function and its parameter. The member "transID" and "seqNum" can told your program what are the "transID" and "seqNum" of your current ZDP-Command when ZDP-Request-Function returned. make sure that ZDP-trans-ID is not ZDP-sequence-number. 






















