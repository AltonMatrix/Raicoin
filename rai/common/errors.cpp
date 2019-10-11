#include <rai/common/errors.hpp>

#include <string>

std::string rai::ErrorString(rai::ErrorCode error_code)
{
    switch (error_code)
    {
        case rai::ErrorCode::SUCCESS:
        {
            return "Success";
        }
        case rai::ErrorCode::GENERIC:
        {
            return "Unknown error";
        }
        case rai::ErrorCode::STREAM:
        {
            return "Invalid stream";
        }
        case rai::ErrorCode::BLOCK_TYPE:
        {
            return "Invalid block type";
        }
        case rai::ErrorCode::BLOCK_OPCODE:
        {
            return "Invalid block opcode";
        }
        case rai::ErrorCode::NOTE_LENGTH:
        {
            return "Invalid note length";
        }
        case rai::ErrorCode::UNKNOWN_COMMAND:
        {
            return "Unknown command";
        }
        case rai::ErrorCode::DATA_PATH:
        {
            return "Failed to create data directory, please check <data_path>";
        }
        case rai::ErrorCode::CONFIG_VERSION:
        {
            return "Unknown daemon config version";
        }
        case rai::ErrorCode::OPEN_OR_CREATE_FILE:
        {
            return "Failed to open or create file";
        }
        case rai::ErrorCode::WRITE_FILE:
        {
            return "Failed to write file";
        }
        case rai::ErrorCode::CONFIG_NODE_VERSION:
        {
            return "Unknown node config version";
        }
        case rai::ErrorCode::CONFIG_LOG_VERSION:
        {
            return "Unknown log config version";
        }
        case rai::ErrorCode::PASSWORD_LENGTH:
        {
            return "Error: password length must be 8 to 256";
        }
        case rai::ErrorCode::PASSWORD_MATCH:
        {
            return "Error: passwords do not match";
        }
        case rai::ErrorCode::KEY_FILE_EXIST:
        {
            return "The specified key file already exists";
        }
        case rai::ErrorCode::DERIVE_KEY:
        {
            return "Failed to derive key from password";
        }
        case rai::ErrorCode::CMD_MISS_KEY:
        {
            return "Please specify key parameter to run the command";
        }
        case rai::ErrorCode::KEY_FILE_NOT_EXIST:
        {
            return "Can not find the specified key file";
        }
        case rai::ErrorCode::KEY_FILE_VERSION:
        {
            return "Unknown key file format";
        }
        case rai::ErrorCode::INVALID_KEY_FILE:
        {
            return "Invalid key file";
        }
        case rai::ErrorCode::PASSWORD_ERROR:
        {
            return "Password incorrect";
        }
        case rai::ErrorCode::MAGIC_NUMBER:
        {
            return "Invalid magic number";
        }
        case rai::ErrorCode::INVALID_MESSAGE:
        {
            return "Invalid message type";
        }
        case rai::ErrorCode::UNKNOWN_MESSAGE:
        {
            return "Unknown message type";
        }
        case rai::ErrorCode::HANDSHAKE_TYPE:
        {
            return "Unknow handshake type";
        }
        case rai::ErrorCode::OUTDATED_VERSION:
        {
            return "Outdated message version";
        }
        case rai::ErrorCode::UNKNOWN_VERSION:
        {
            return "Unknown message version";
        }
        case rai::ErrorCode::SIGNATURE:
        {
            return "Bad signature";
        }
        case rai::ErrorCode::KEEPLIVE_PEERS:
        {
            return "More than maximum allowed peers in keeplive message";
        }
        case rai::ErrorCode::KEEPLIVE_REACHABLE_PEERS:
        {
            return "Reachable peers more than total peers in keeplive message";
        }
        case rai::ErrorCode::CONFIG_RPC_VERSION:
        {
            return "Unknow rpc config version";
        }
        case rai::ErrorCode::PUBLISH_NEED_CONFIRM:
        {
            return "Invalid extension field in publish message";
        }
        case rai::ErrorCode::BLOCK_CREDIT:
        {
            return "Invalid block credit field";
        }
        case rai::ErrorCode::BLOCK_COUNTER:
        {
            return "Invalid block counter field";
        }
        case rai::ErrorCode::BLOCK_TIMESTAMP:
        {
            return "Invalid block timestamp field";
        }
        case rai::ErrorCode::MDB_ENV_CREATE:
        {
            return "Failed to create MDB environment";
        }
        case rai::ErrorCode::MDB_ENV_SET_MAXDBS:
        {
            return "Failed to set MDB environment max datebases";
        }
        case rai::ErrorCode::MDB_ENV_SET_MAPSIZE:
        {
            return "Failed to set MDB environment map size";
        }
        case rai::ErrorCode::MDB_ENV_OPEN:
        {
            return "Failed to open MDB environment";
        }
        case rai::ErrorCode::MDB_TXN_BEGIN:
        {
            return "Failed to begin MDB transaction";
        }
        case rai::ErrorCode::MDB_DBI_OPEN:
        {
            return "Failed to open MDB database";
        }
        case rai::ErrorCode::MESSAGE_QUERY_BY:
        {
            return "Query message with invalid query_by field";
        }
        case rai::ErrorCode::MESSAGE_QUERY_STATUS:
        {
            return "Query message with invalid query_status field";
        }
        case rai::ErrorCode::MESSAGE_QUERY_BLOCK:
        {
            return "Query message with invalid block";
        }
        case rai::ErrorCode::MESSAGE_CONFIRM_SIGNATURE:
        {
            return "Confirm message with invalid signature";
        }
        case rai::ErrorCode::MESSAGE_FORK_ACCOUNT:
        {
            return "Fork message with unequal accounts";
        }
        case rai::ErrorCode::MESSAGE_FORK_HEIGHT:
        {
            return "Fork message with unequal heights";
        }
        case rai::ErrorCode::MESSAGE_FORK_BLOCK_EQUAL:
        {
            return "Fork message with same blocks";
        }
        case rai::ErrorCode::MESSAGE_CONFLICT_BLOCK:
        {
            return "Conflict message with invalid block";
        }
        case rai::ErrorCode::MESSAGE_CONFLICT_SIGNATURE:
        {
            return "Conflict message with invalid signature";
        }
        case rai::ErrorCode::MESSAGE_CONFLICT_TIMESTAMP:
        {
            return "Conflict message with invalid timestamp";
        }
        case rai::ErrorCode::ACCOUNT_LIMITED:
        {
            return "Account is limited";
        }
        case rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET:
        {
            return "Failed to get account infomation from ledger";;
        }
        case rai::ErrorCode::LEDGER_BLOCK_GET:
        {
            return "Failed to get block from ledger";
        }
        case rai::ErrorCode::CMD_MISS_HASH:
        {
            return "Please specify hash parameter to run the command";
        }
        case rai::ErrorCode::CMD_INVALID_HASH:
        {
            return "Invalid hash parameter";
        }
        case rai::ErrorCode::LEDGER_RECEIVABLE_INFO_COUNT:
        {
            return "Failed to get receivable count from ledger";
        }
        case rai::ErrorCode::LEDGER_BLOCK_COUNT:
        {
            return "Failed to get block count from ledger";
        }
        case rai::ErrorCode::LEDGER_ACCOUNT_COUNT:
        {
            return "Failed to get account count from ledger";
        }
        case rai::ErrorCode::BOOTSTRAP_PEER:
        {
            return "Failed to get peer for bootstrap";
        }
        case rai::ErrorCode::JSON_GENERIC:
        {
            return "Failed to parse json";
        }
        case rai::ErrorCode::JSON_BLOCK_TYPE:
        {
            return "Failed to parse block type from json";
        }
        case rai::ErrorCode::JSON_BLOCK_OPCODE:
        {
            return "Failed to parse block opcode from json";
        }
        case rai::ErrorCode::JSON_BLOCK_CREDIT:
        {
            return "Failed to parse block credit from json";
        }
        case rai::ErrorCode::JSON_BLOCK_COUNTER:
        {
            return "Failed to parse block counter from json";
        }
        case rai::ErrorCode::JSON_BLOCK_TIMESTAMP:
        {
            return "Failed to parse block timestamp from json";
        }
        case rai::ErrorCode::JSON_BLOCK_HEIGHT:
        {
            return "Failed to parse block height from json";
        }
        case rai::ErrorCode::JSON_BLOCK_ACCOUNT:
        {
            return "Failed to parse block account from json";
        }
        case rai::ErrorCode::JSON_BLOCK_PREVIOUS:
        {
            return "Failed to parse block previous from json";
        }
        case rai::ErrorCode::JSON_BLOCK_REPRESENTATIVE:
        {
            return "Failed to parse block representative from json";
        }
        case rai::ErrorCode::JSON_BLOCK_BALANCE:
        {
            return "Failed to parse block balance from json";
        }
        case rai::ErrorCode::JSON_BLOCK_LINK:
        {
            return "Failed to parse block link from json";
        }
        case rai::ErrorCode::JSON_BLOCK_NOTE_LENGTH:
        {
            return "Failed to parse block note length from json";
        }
        case rai::ErrorCode::JSON_BLOCK_NOTE_TYPE:
        {
            return "Failed to parse block note type from json";
        }
        case rai::ErrorCode::JSON_BLOCK_NOTE_ENCODE:
        {
            return "Failed to parse block note encode from json";
        }
        case rai::ErrorCode::JSON_BLOCK_NOTE_DATA:
        {
            return "Failed to parse block note data from json";
        }
        case rai::ErrorCode::JSON_BLOCK_SIGNATURE:
        {
            return "Failed to parse block signature from json";
        }
        case rai::ErrorCode::JSON_BLOCK_PRICE:
        {
            return "Failed to parse credit price from json";
        }
        case rai::ErrorCode::JSON_BLOCK_BEGIN_TIME:
        {
            return "Failed to parse begin time from json";
        }
        case rai::ErrorCode::JSON_BLOCK_END_TIME:
        {
            return "Failed to parse end time from json";
        }
        case rai::ErrorCode::JSON_CONFIG_VERSION:
        {
            return "Failed to parse version from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC:
        {
            return "Failed to parse rpc from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_NODE:
        {
            return "Failed to parse node from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_NODE_VERSION:
        {
            return "Failed to parse node version from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG:
        {
            return "Failed to parse log from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_VERSION:
        {
            return "Failed to parse log version from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_TO_CERR:
        {
            return "Failed to parse log_to_cerr from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_MAX_SIZE:
        {
            return "Failed to parse log max_size from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_ROTATION_SIZE:
        {
            return "Failed to parse log rotation_size from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_FLUSH:
        {
            return "Failed to parse log flush from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_NODE_PORT:
        {
            return "Failed to parse node port from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_NODE_IO_THREADS:
        {
            return "Failed to parse node io_threads from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_NETWORK:
        {
            return "Failed to parse log network from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_NETWORK_SEND:
        {
            return "Failed to parse log network_send from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_NETWORK_RECEIVE:
        {
            return "Failed to parse log network_receive from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_MESSAGE:
        {
            return "Failed to parse log message from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_MESSAGE_HANDSHAKE:
        {
            return "Failed to parse log message_handshake from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_PRECONFIGURED_PEERS:
        {
            return "Failed to parse node preconfigured_peers from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC_ENABLE:
        {
            return "Failed to parse rpc enable from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC_VERSION:
        {
            return "Failed to parse rpc version from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC_ADDRESS:
        {
            return "Failed to parse rpc address from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC_PORT:
        {
            return "Failed to parse rpc port from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC_ENABLE_CONTROL:
        {
            return "Failed to parse rpc enable_control from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC_WHITELIST:
        {
            return "Failed to parse rpc whitelist from config.json";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_RPC:
        {
            return "Failed to parse log rpc from config.json";
        }
        case rai::ErrorCode::RPC_GENERIC:
        {
            return "Internal server error in RPC";
        }
        case rai::ErrorCode::RPC_JSON:
        {
            return "Invalid JSON format";
        }
        case rai::ErrorCode::RPC_JSON_DEPTH:
        {
            return "Max JSON depth exceeded";
        }
        case rai::ErrorCode::RPC_HTTP_BODY_SIZE:
        {
            return "Max http body size exceeded";
        }
        case rai::ErrorCode::RPC_NOT_LOCALHOST:
        {
            return "The action is only allowed on localhost(127.0.0.1)";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_BLOCK:
        {
            return "The block field is missing";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_ACCOUNT:
        {
            return "The account field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_ACCOUNT:
        {
            return "Invalid account field";
        }
        case rai::ErrorCode::RPC_ACCOUNT_NOT_EXIST:
        {
            return "The account does not exist";
        }
        case rai::ErrorCode::BLOCK_PROCESS_GENERIC:
        {
            return "Error in block processor";
        }
        case rai::ErrorCode::BLOCK_PROCESS_SIGNATURE:
        {
            return "Invalid block signature";
        }
        case rai::ErrorCode::BLOCK_PROCESS_EXISTS:
        {
            return "Block has already been processed";
        }
        case rai::ErrorCode::BLOCK_PROCESS_GAP_PREVIOUS:
        {
            return "Previous block does not exist";
        }
        case rai::ErrorCode::BLOCK_PROCESS_GAP_RECEIVE_SOURCE:
        {
            return "Receive source block does not exist";
        }
        case rai::ErrorCode::BLOCK_PROCESS_GAP_REWARD_SOURCE:
        {
            return "Reward source block does not exist";
        }
        case rai::ErrorCode::BLOCK_PROCESS_PREVIOUS:
        {
            return "Invalid block previous";
        }
        case rai::ErrorCode::BLOCK_PROCESS_OPCODE:
        {
            return "Invalid block opcode";
        }
        case rai::ErrorCode::BLOCK_PROCESS_CREDIT:
        {
            return "Invalid block credit";
        }
        case rai::ErrorCode::BLOCK_PROCESS_COUNTER:
        {
            return "Invalid block counter";
        }
        case rai::ErrorCode::BLOCK_PROCESS_TIMESTAMP:
        {
            return "Invalid block timestamp";
        }
        case rai::ErrorCode::BLOCK_PROCESS_BALANCE:
        {
            return "Invalid block balance";
        }
        case rai::ErrorCode::BLOCK_PROCESS_UNRECEIVABLE:
        {
            return "Source block is unreceivable";
        }
        case rai::ErrorCode::BLOCK_PROCESS_UNREWARDABLE:
        {
            return "Source block is unrewardable";
        }
        case rai::ErrorCode::BLOCK_PROCESS_PRUNED:
        {
            return "Block has been pruned";
        }
        case rai::ErrorCode::BLOCK_PROCESS_FORK:
        {
            return "Fork detected";
        }
        case rai::ErrorCode::BLOCK_PROCESS_TYPE_MISMATCH:
        {
            return "Block type mismatch";
        }
        case rai::ErrorCode::BLOCK_PROCESS_REPRESENTATIVE:
        {
            return "Invalid representative";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LINK:
        {
            return "Invalid block link";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_PUT:
        {
            return "Failed to put block to ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_SUCCESSOR_SET:
        {
            return "Failed to set block successor to ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET:
        {
            return "Failed to get block from ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_TYPE_UNKNOWN:
        {
            return "Unknown block type";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_PUT:
        {
            return "Failed to put receivable infomation to ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_DEL:
        {
            return "Failed to delete receivable infomation from ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ACCOUNT_EXCEED_TRANSACTIONS:
        {
            return "Account exceeds maximum transactions";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_PUT:
        {
            return "Failed to put rewardable infomation to ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT:
        {
            return "Failed to put account infomation to ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT:
        {
            return "Data inconsistency in the ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_DEL:
        {
            return "Failed to delete block from ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_ROLLBACK_BLOCK_PUT:
        {
            return "Failed to put rollback block to ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_DEL:
        {
            return "Failed to delete account infomation from ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_REWARDED:
        {
            return "Rollback rewarded block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_CONFIRM_BLOCK_MISS:
        {
            return "Confirm missing block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_SOURCE_PRUNED:
        {
            return "Rollback block with pruned source";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_NOT_EQUAL_TO_HEAD:
        {
            return "Rollback block not equal to head block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_RECEIVED:
        {
            return "Rollback received block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_IGNORE:
        {
            return "Rollback operation ignored";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_NON_HEAD:
        {
            return "Rollback non-head block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_TAIL:
        {
            return "Rollback tail block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_PREPEND_IGNORE:
        {
            return "Prepend operation ignored";
        }
        case rai::ErrorCode::BLOCK_PROCESS_UNKNOWN_OPERATION:
        {
            return "Unknown block operation";
        }
        case rai::ErrorCode::BLOCK_PROCESS_CONTINUE:
        {
            return "Continue processing block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_WAIT:
        {
            return "Wait missing block";
        }
        default:
        {
            return "Invalid error code";
        }
    }
}