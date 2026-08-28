#pragma once

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "datacenter.h"
#include "dc_crc16.h"
#include "dc_test_storage.h"
#include "dc_test_variable.h"
#include "dc_variable.h"
}

/// @brief 向 A 区 EE 槽写入 CRC 合法块，首数据字节为 first_byte。
inline void SeedAClassEeSlot(E_VARIABLE_EE_SLOT slot, uint8_t first_byte)
{
    uint8_t block[64];
    uint16_t crc;

    std::memset(block, 0, sizeof block);
    block[0] = first_byte;
    crc = dc_crc16_ccitt(block, VAR_A_CRC_ADDR);
    block[VAR_A_CRC_ADDR] = static_cast<uint8_t>(crc & 0xFFu);
    block[VAR_A_CRC_ADDR + 1u] = static_cast<uint8_t>(crc >> 8);
    ASSERT_EQ(VariableEeWriteSlot(slot, block, VAR_A_END_ADDR),
              static_cast<int16_t>(VAR_A_END_ADDR));
}

/// @brief 向 B 区 EE 槽写入 CRC 合法块，首数据字节为 first_byte。
inline void SeedBClassEeSlot(E_VARIABLE_EE_SLOT slot, uint8_t first_byte)
{
    uint8_t block[64];
    uint16_t crc;

    std::memset(block, 0, sizeof block);
    block[0] = first_byte;
    crc = dc_crc16_ccitt(block, VAR_B_CRC_ADDR);
    block[VAR_B_CRC_ADDR] = static_cast<uint8_t>(crc & 0xFFu);
    block[VAR_B_CRC_ADDR + 1u] = static_cast<uint8_t>(crc >> 8);
    ASSERT_EQ(VariableEeWriteSlot(slot, block, VAR_B_END_ADDR),
              static_cast<int16_t>(VAR_B_END_ADDR));
}

inline void SeedAClassEe(uint8_t first_byte)
{
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_ON_0, first_byte);
}

/// @brief 映射表中单元素最大字节数（随 cfg 自动扩展）。
inline uint16_t MaxVarElementBytes(void)
{
    uint16_t max_bytes = 1u;

    for (uint16_t i = 0u; i < tVariableApiTableCount; ++i)
    {
        if (tVariableApiTable[i].ucBytes > max_bytes)
        {
            max_bytes = tVariableApiTable[i].ucBytes;
        }
    }
    return max_bytes;
}

/// @brief 分配读写缓冲区，长度为当前映射表单元素上限。
inline std::vector<uint8_t> MakeVarIoBuffer(void)
{
    return std::vector<uint8_t>(MaxVarElementBytes(), 0u);
}

/// @brief 按 row/index 生成可识别的写入图案，便于读写回读比对。
inline void FillVarWritePattern(uint8_t *buf, uint16_t nbytes, uint16_t row, uint8_t index)
{
    for (uint16_t b = 0u; b < nbytes; ++b)
    {
        buf[b] = static_cast<uint8_t>(0xA0u + static_cast<uint8_t>(row & 0x0Fu) + index +
                                      static_cast<uint8_t>(b & 0x0Fu));
    }
}

inline int16_t ReadVar(uint16_t type_id, uint8_t index, uint8_t *buf, uint16_t len)
{
    return dc_read_alias(VarAliasBuild(type_id, index), buf, len, 0u);
}

inline int16_t WriteVar(uint16_t type_id, uint8_t index, const uint8_t *buf, uint16_t len)
{
    return dc_write_alias(VarAliasBuild(type_id, index), buf, len, 0u);
}

inline void ExpectZoneBodyCrcOk(dc_test_var_zone_t zone)
{
    EXPECT_NE(DcTestVarBodyCrcOk(zone), 0);
}

inline void InitVariableModule(void)
{
    std::vector<uint8_t> buf(MakeVarIoBuffer());

    SeedAClassEe(0u);
    ReadVar(VARIABLE_DATE_TIME, 0, buf.data(), 1u);
}

inline void ExpectEeSlotFirstByte(E_VARIABLE_EE_SLOT slot, uint8_t expected, uint16_t block_len)
{
    uint8_t block[64];

    ASSERT_EQ(VariableEeReadSlot(slot, block, block_len), static_cast<int16_t>(block_len));
    EXPECT_EQ(block[0], expected);
}

class VariableTestBase : public ::testing::Test {
protected:
    void SetUp() override
    {
        DcTestStorageReset();
        DcTestVarReset();
    }
};
