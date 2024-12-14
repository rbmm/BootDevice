#include "stdafx.h"

_NT_BEGIN

#include "print.h"

extern const volatile UCHAR guz = 0; // /RTCs must not be set

NTSTATUS PrintDiskInfo(ULONG DiskNumber)
{
	WCHAR sz[32];
	if (0 >= swprintf_s(sz, _countof(sz), L"\\GLOBAL??\\PhysicalDrive%u", DiskNumber))
	{
		return STATUS_INTERNAL_ERROR;
	}

	UNICODE_STRING ObjectName;
	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName, OBJ_CASE_INSENSITIVE };
	IO_STATUS_BLOCK iosb;
	HANDLE hFile;
	RtlInitUnicodeString(&ObjectName, sz);
	NTSTATUS status = NtOpenFile(&hFile, SYNCHRONIZE, &oa, &iosb, FILE_SHARE_VALID_FLAGS, FILE_SYNCHRONOUS_IO_NONALERT);

	if (0 <= status)
	{
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
					status = STATUS_INTERNAL_ERROR;
					break;
				}
				cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
			}

			status = NtDeviceIoControlFile(hFile, 0, 0, 0, &iosb, 
				IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq), buf, cb);

			if (cb < (rcb = psdd->Size))
			{
				status = STATUS_BUFFER_OVERFLOW;
			}

		} while (STATUS_BUFFER_OVERFLOW == status);

		NtClose(hFile);

		if (0 <= status)
		{
			DbgPrint("[%u]: %hs%hs [%hs]\r\n", DiskNumber, 
				psdd->VendorIdOffset ? (PSTR)psdd + psdd->VendorIdOffset : "",
				psdd->ProductIdOffset ? (PSTR)psdd + psdd->ProductIdOffset : "",
				psdd->ProductRevisionOffset ? (PSTR)psdd + psdd->ProductRevisionOffset : "");
		}
	}

	if (0 > status)
	{
		PrintError(status);
	}

	return status;
}

NTSTATUS GetSysDrive(PCWSTR psz)
{
	UNICODE_STRING ObjectName;
	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName, OBJ_CASE_INSENSITIVE };
	IO_STATUS_BLOCK iosb;
	HANDLE hFile;
	RtlInitUnicodeString(&ObjectName, psz);
	NTSTATUS status = NtOpenFile(&hFile, SYNCHRONIZE, &oa, &iosb, FILE_SHARE_VALID_FLAGS, FILE_SYNCHRONOUS_IO_NONALERT);

	if (0 <= status)
	{
		STORAGE_DEVICE_NUMBER sdn;

		if (0 <= (status = NtDeviceIoControlFile(hFile, 0, 0, 0, &iosb, IOCTL_STORAGE_GET_DEVICE_NUMBER, 0, 0, &sdn, sizeof(sdn))))
		{
			DbgPrint("%ws: Device=%u, Partition=%u\r\n", psz, sdn.DeviceNumber, sdn.PartitionNumber);
			PrintDiskInfo(sdn.DeviceNumber);
		}

		union {
			PVOLUME_DISK_EXTENTS pvde;
			PVOID buf;
		};
		PVOID stack = alloca(guz);
		ULONG cb = 0, rcb = sizeof(VOLUME_DISK_EXTENTS);
		do 
		{
			if (cb < rcb)
			{
				if (rcb > 0x4000)
				{
					status = STATUS_INTERNAL_ERROR;
					break;
				}
				cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
			}

			status = NtDeviceIoControlFile(hFile, 0, 0, 0, &iosb, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, buf, cb);

			rcb = offsetof(VOLUME_DISK_EXTENTS, Extents) + pvde->NumberOfDiskExtents * sizeof(DISK_EXTENT);

		} while (STATUS_BUFFER_OVERFLOW == status);

		NtClose(hFile);

		if (0 <= status)
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
	}

	return status;
}

void WINAPI ep(void*)
{
	PrintInfo pi;
	InitPrintf();

	PrintError(GetSysDrive(L"\\Device\\BootPartition"));
	PrintError(GetSysDrive(L"\\Device\\SystemPartition"));
	
	ExitProcess(0);
}

_NT_END