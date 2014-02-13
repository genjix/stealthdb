#include "stealth_database.hpp"
using namespace libbitcoin;

#include <bitcoin/bitcoin.hpp>

#include "txs.hpp"

struct stealth_result_row
{
    data_chunk ephemkey;
    payment_address address;
    hash_digest transaction_id;
};

void create_file(const std::string& filename, size_t filesize)
{
    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    constexpr size_t chunk_size = 100000;
    std::vector<char> random_buffer(chunk_size);
    for (size_t i = 0; i < filesize; i += chunk_size)
        file.write(random_buffer.data(), chunk_size);
}

void initialize_new(const std::string& filename)
{
    create_file(filename, 100000000);
    mmfile file(filename);
    auto serial = make_serializer(file.data());
    serial.write_4_bytes(1);
    // should last us a decade
    size_t max_header_rows = 10000;
    serial.write_4_bytes(max_header_rows);
    serial.write_4_bytes(0);
    for (size_t i = 0; i < max_header_rows; ++i)
        serial.write_4_bytes(0);
}

bool is_stealth_script(const operation_stack& ops)
{
    return ops.size() == 2 &&
        ops[0].code == opcode::return_ &&
        ops[1].code == opcode::special &&
        ops[1].data.size() == 1 + 4 + 33 &&
        ops[1].data[0] == 0x06 &&
        (ops[1].data[5] == 0x02 || ops[1].data[5] == 0x03);
}

int main()
{
    initialize_new("stealth.db");
    mmfile file("stealth.db");
    stealth_database db(file);
    auto write_func = [](uint8_t* it)
    {
        auto serial = make_serializer(it);
        // prefix.bitfield
        serial.write_4_bytes(0xdeadbeef);
        serial.write_data(decode_hex(
            "034645240b789fe5caa3f01cd32a4927eb"
            "057d947fe19592477959e5f39a328319"));
        serial.write_byte(0x06);
        serial.write_data(decode_hex(
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
        serial.write_data(decode_hex(
            "63e75e43de21b73d7eb0220ce44dcfa5f"
            "c7717a8decebb254b31ef13047fa518"));
        constexpr uint32_t entry_row_size = 4 + 33 + 21 + 32;
        BITCOIN_ASSERT(serial.iterator() == it + entry_row_size);
    };
    //db.store(write_func);
    //db.sync(0);
    //db.store(write_func);
    //db.store(write_func);
    //db.sync(100);

    size_t i = 0;
    for (const data_chunk& raw_tx: txs)
    {
        transaction_type tx;
        satoshi_load(raw_tx.begin(), raw_tx.end(), tx);
        hash_digest tx_hash = hash_transaction(tx);
        data_chunk stealth_data;
        const data_chunk* stealth_data2 = nullptr;
        stealth_data2 = &stealth_data;
        for (const auto& output: tx.outputs)
        {
            if (is_stealth_script(output.script.operations()))
            {
                stealth_data = output.script.operations()[1].data;
                continue;
            }
            payment_address address;
            if (!extract(address, output.script))
            {
                stealth_data.clear();
                continue;
            }
            if (!stealth_data.empty())
            {
                // This is a stealth output.
                hash_digest index = generate_sha256_hash(stealth_data);
                auto deserial = make_deserializer(index.begin(), index.begin() + 4);
                uint32_t bitfield = deserial.read_4_bytes();
                BITCOIN_ASSERT(stealth_data.size() == 1 + 4 + 33);
                data_chunk ephemkey(stealth_data.begin() + 5, stealth_data.end());
                BITCOIN_ASSERT(ephemkey.size() == 33);
                auto write_func = [bitfield, ephemkey, address, tx_hash](uint8_t *it)
                {
                    auto serial = make_serializer(it);
                    serial.write_4_bytes(bitfield);
                    serial.write_data(ephemkey);
                    serial.write_byte(address.version());
                    serial.write_short_hash(address.hash());
                    serial.write_hash(tx_hash);
                    BITCOIN_ASSERT(serial.iterator() == it + 4 + 33 + 21 + 32);
                };
                db.store(write_func);
                stealth_data.clear();
            }
        }
        i += 50;
        db.sync(i);
    }

    auto read_func = [](const uint8_t* it)
    {
        constexpr uint32_t row_size = 4 + 33 + 21 + 32;
        auto deserial = make_deserializer(it, it + row_size);
        data_chunk bitfield = deserial.read_data(4);
        data_chunk ephemkey = deserial.read_data(33);
        uint8_t version = deserial.read_byte();
        short_hash hash = deserial.read_short_hash();
        hash_digest tx_hash = deserial.read_hash();
        std::cout << "Bitfield: " << bitfield
            << " ephemkey: " << ephemkey
            << " address: (" << (int)version << ", " << hash << ")"
            << " tx_hash: " << tx_hash << std::endl;
    };
    db.scan(read_func, 0);
    return 0;
}

