#ifndef _GRBL
#define _GRBL

#define GRBL_STATUS_ENUM \
ADD_ENUM(eStatus)\
ADD_ENUM(eRPosX)\
ADD_ENUM(eRPosY)\
ADD_ENUM(eRPosZ)\
ADD_ENUM(eFeed)\
ADD_ENUM(eSpindle)\
ADD_ENUM(ePlanner)\
ADD_ENUM(eRXBytes)\
ADD_ENUM(eLineNumber)\
ADD_ENUM(eOVFeed)\
ADD_ENUM(eOVRapid)\
ADD_ENUM(eOVSpindle)\
ADD_ENUM(eWCOX)\
ADD_ENUM(eWCOY)\
ADD_ENUM(eWCOZ)\
ADD_ENUM(ePins)

enum grblStatusT
{
#define ADD_ENUM(eVal) eVal,
   GRBL_STATUS_ENUM
#undef ADD_ENUM
   eStatusRange
};

#endif