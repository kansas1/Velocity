#ifndef XCONTENTDEVICEITEM_H
#define XCONTENTDEVICEITEM_H

#include <iostream>
#include "Stfs/StfsPackage.h"

#include "XboxInternals_global.h"

class XBOXINTERNALSSHARED_EXPORT XContentDeviceItem
{
public:
    XContentDeviceItem(std::string pathOnDevice, StfsPackage *package);

    StfsPackage *package;

    virtual std::string GetPathOnDevice();
    virtual std::wstring GetName();
    virtual BYTE* GetThumbnail();
    virtual DWORD GetThumbnailSize();

private:
    std::string pathOnDevice;
};

#endif // XCONTENTDEVICEITEM_H