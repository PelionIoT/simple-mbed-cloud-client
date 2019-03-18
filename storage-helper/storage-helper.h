// ----------------------------------------------------------------------------
// Copyright 2016-2018 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------

#ifndef SIMPLEMBEDCLOUDCLIENT_STORAGEHELPER_H_
#define SIMPLEMBEDCLOUDCLIENT_STORAGEHELPER_H_

#include "mbed.h"
#include "BlockDevice.h"
#include "FileSystem.h"
#include "factory_configurator_client.h"

// This is for single or dual partition mode. This is supposed to be used with storage for data e.g. SD card.
// Enable by 1/disable by 0.
#ifndef MCC_PLATFORM_PARTITION_MODE
#define MCC_PLATFORM_PARTITION_MODE 0
#endif

#include "pal.h"
#if (MCC_PLATFORM_PARTITION_MODE == 1)
#include "MBRBlockDevice.h"
#include "FATFileSystem.h"

// Set to 1 for enabling automatic partitioning storage if required. This is effective only if MCC_PLATFORM_PARTITION_MODE is defined to 1.
// Partioning will be triggered only if initialization of available partitions fail.
#ifndef MCC_PLATFORM_AUTO_PARTITION
#define MCC_PLATFORM_AUTO_PARTITION 0
#endif

#ifndef PRIMARY_PARTITION_NUMBER
#define PRIMARY_PARTITION_NUMBER 1
#endif

#ifndef PRIMARY_PARTITION_START
#define PRIMARY_PARTITION_START 0
#endif

#ifndef PRIMARY_PARTITION_SIZE
#define PRIMARY_PARTITION_SIZE 1024*1024*1024 // default partition size 1GB
#endif

#ifndef SECONDARY_PARTITION_NUMBER
#define SECONDARY_PARTITION_NUMBER 2
#endif

#ifndef SECONDARY_PARTITION_START
#define SECONDARY_PARTITION_START PRIMARY_PARTITION_SIZE
#endif

#ifndef SECONDARY_PARTITION_SIZE
#define SECONDARY_PARTITION_SIZE 1024*1024*1024 // default partition size 1GB
#endif

#ifndef NUMBER_OF_PARTITIONS
#define NUMBER_OF_PARTITIONS PAL_NUMBER_OF_PARTITIONS
#endif

#ifndef MOUNT_POINT_PRIMARY
#define MOUNT_POINT_PRIMARY PAL_FS_MOUNT_POINT_PRIMARY
#endif

#ifndef MOUNT_POINT_SECONDARY
#define MOUNT_POINT_SECONDARY PAL_FS_MOUNT_POINT_SECONDARY
#endif

#endif // MCC_PLATFORM_PARTITION_MODE

// Include this only for Developer mode and device which doesn't have in-built TRNG support
#if MBED_CONF_DEVICE_MANAGEMENT_DEVELOPER_MODE == 1
#ifdef PAL_USER_DEFINED_CONFIGURATION
#define FCC_ROT_SIZE                       16
const uint8_t MBED_CLOUD_DEV_ROT[FCC_ROT_SIZE] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
#if !PAL_USE_HW_TRNG
#define FCC_ENTROPY_SIZE                   48
const uint8_t MBED_CLOUD_DEV_ENTROPY[FCC_ENTROPY_SIZE] = { 0xf6, 0xd6, 0xc0, 0x09, 0x9e, 0x6e, 0xf2, 0x37, 0xdc, 0x29, 0x88, 0xf1, 0x57, 0x32, 0x7d, 0xde, 0xac, 0xb3, 0x99, 0x8c, 0xb9, 0x11, 0x35, 0x18, 0xeb, 0x48, 0x29, 0x03, 0x6a, 0x94, 0x6d, 0xe8, 0x40, 0xc0, 0x28, 0xcc, 0xe4, 0x04, 0xc3, 0x1f, 0x4b, 0xc2, 0xe0, 0x68, 0xa0, 0x93, 0xe6, 0x3a };
#endif // PAL_USE_HW_TRNG = 0
#endif // PAL_USER_DEFINED_CONFIGURATION
#endif // #if MBED_CONF_DEVICE_MANAGEMENT_DEVELOPER_MODE == 1

class StorageHelper {
public:
    /**
     * Initializes a new StorageHelper.
     * StorageHelper manages storage across multiple partitions,
     * initializes SOTP, and can format the storage.
     *
     * @param bd An un-initialized block device to back the file system
     * @param fs An un-mounted file system
     */
    StorageHelper(BlockDevice *bd, FileSystem *fs);

    /**
     * Initialize the storage helper, this initializes and mounts
     * both the block device and file system
     *
     * @returns 0 if successful, non-0 when not successful
     */
    int init();

    /**
     * Initialize the factory configurator client, sets entropy,
     * and reads root of trust.
     *
     * @returns 0 if successful, non-0 when not successful
     */
    int sotp_init();

    /**
     * Format the block device, and remount the filesystem
     *
     * @returns 0 if successful, non-0 when not successful
     */
    int reformat_storage(void);

    /**
     * Initialize and format a blockdevice and file system
     */
    static int format(FileSystem *fs, BlockDevice *bd);

private:
#if (MCC_PLATFORM_PARTITION_MODE == 1)
    // for checking that PRIMARY_PARTITION_SIZE and SECONDARY_PARTITION_SIZE do not overflow.
    bd_size_t mcc_platform_storage_size;

    /**
     * Initialize and mount the partition on the file system
     * The block device must be initialized before calling this function.
     *
     * @param fs Pointer to an array of file systems, one per partition
     * @param part Pointer to an array of block devices, one per partition.
     *              All these need to be initialized.
     * @param number_of_partitions Total number of partitions
     * @param mount_point Mount point
     *
     * @returns 0 if successful, non-0 when not successful
     */
    int init_and_mount_partition(FileSystem **fs, BlockDevice** part, int number_of_partition, const char* mount_point);
#endif

#if ((MCC_PLATFORM_PARTITION_MODE == 1) && (MCC_PLATFORM_AUTO_PARTITION == 1))
    int create_partitions(void);
#endif
    /**
     * Reformat a single partition
     *
     * @param fs A file system
     * @param part A block device
     *
     * @returns 0 if successful, non-0 when not successful
     */
    int reformat_partition(FileSystem *fs, BlockDevice* part);

    /**
     * Test whether the file system is functional.
     * This unmounts, then mounts the file system against the block device
     *
     * If the file system cannot be unmounted, this will be ignored.
     *
     * @param fs A file system
     * @param part A block device
     *
     * @returns 0 if successful, non-0 when not successful
     */
    int test_filesystem(FileSystem *fs, BlockDevice* part);

    BlockDevice *_bd;
    FileSystem *_fs;

    FileSystem *fs1;
    FileSystem *fs2;
    BlockDevice *part1;
    BlockDevice *part2;
};

#endif // SIMPLEMBEDCLOUDCLIENT_STORAGEHELPER_H_
