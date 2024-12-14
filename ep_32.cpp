#include "stdafx.h"

_NT_BEGIN

#include "print.h"

extern const volatile UCHAR guz = 0; // /RTCs must not be set

//#define RtlPointerToOffset(B,P) ((ULONG)( ((PCHAR)(P)) - ((PCHAR)(B)) ))

void PrintDiskInfo(ULONG DiskNumber)
{
	WCHAR sz[32];
	if (0 >= swprintf_s(sz, _countof(sz), L"\\\\?\\PhysicalDrive%u", DiskNumber))
	{
		return ;
	}

	if (HANDLE hFile = fixH(CreateFileW(sz, 0, 0, 0, OPEN_EXISTING, 0, 0)))
	{
		ULONG dwError;

		STORAGE_PROPERTY_QUERY spq = { StorageDeviceProperty, PropertyStandardQuery };

		union {
			PSTORAGE_DEVICE_DESCRIPTOR psdd;
			PVOID buf;
		};
		PVOID stack = alloca(guz);
		ULONG cb = 0, rcb = sizeof(STORAGE_DEVICE_DESCRIPTOR);
		do 
		{
			if (cb < rcb)
			{
				if (rcb > 0x4000)
				{
					dwError = ERROR_INTERNAL_ERROR;
					break;
				}
				cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
			}

			dwError = BOOL_TO_ERROR(DeviceIoControl(hFile, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq), buf, cb, &rcb, 0));

			if (cb < (rcb = psdd->Size))
			{
				dwError = ERROR_MORE_DATA;
			}

		} while (ERROR_MORE_DATA == dwError);

		CloseHandle(hFile);

		if (NOERROR == dwError)
		{
			DbgPrint("[%u]: %hs%hs [%hs]\n", DiskNumber, 
				psdd->VendorIdOffset ? (PSTR)psdd + psdd->VendorIdOffset : "",
				psdd->ProductIdOffset ? (PSTR)psdd + psdd->ProductIdOffset : "",
				psdd->ProductRevisionOffset ? (PSTR)psdd + psdd->ProductRevisionOffset : "");
		}
		else
		{
			PrintError(dwError);
		}
	}
}

void GetSysDrive(PCWSTR lpFileName)
{
	if (HANDLE hFile = fixH(CreateFileW(lpFileName, 0, 0, 0, OPEN_EXISTING, 0, 0)))
	{
		STORAGE_DEVICE_NUMBER sdn;

		if (DeviceIoControl(hFile, IOCTL_STORAGE_GET_DEVICE_NUMBER, 0, 0, &sdn, sizeof(sdn), &sdn.PartitionNumber, 0))
		{
			PrintDiskInfo(sdn.DeviceNumber);
		}

		union {
			PVOLUME_DISK_EXTENTS pvde;
			PVOID buf;
		};

		ULONG dwError;

		PVOID stack = alloca(guz);
		ULONG cb = 0, rcb = sizeof(VOLUME_DISK_EXTENTS);
		do 
		{
			if (cb < rcb)
			{
				if (rcb > 0x4000)
				{
					dwError = ERROR_INTERNAL_ERROR;
					break;
				}

				cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
			}

			dwError = BOOL_TO_ERROR(DeviceIoControl(hFile, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, buf, cb, &rcb, 0));

			rcb = offsetof(VOLUME_DISK_EXTENTS, Extents) + pvde->NumberOfDiskExtents * sizeof(DISK_EXTENT);

		} while (ERROR_MORE_DATA == dwError);

		CloseHandle(hFile);

		if (NOERROR == dwError)
		{
			if (ULONG NumberOfDiskExtents = pvde->NumberOfDiskExtents)
			{
				PDISK_EXTENT Extent = pvde->Extents;
				do 
				{
					DbgPrint("Extent[%u]: %I64u [%I64u]:\r\n", Extent->DiskNumber,
						Extent->StartingOffset.QuadPart, Extent->ExtentLength.QuadPart);
					PrintDiskInfo(Extent->DiskNumber);
				} while (Extent++, --NumberOfDiskExtents);
			}
		}
		PrintError(dwError);
	}
}

void WINAPI ep(void*)
{
	PrintInfo pi;
	InitPrintf();

	GetSysDrive(L"\\\\?\\globalroot\\Device\\BootPartition");
	GetSysDrive(L"\\\\?\\SystemPartition");
	
	ExitProcess(0);
}

_NT_END