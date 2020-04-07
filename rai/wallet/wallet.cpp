#include <rai/wallet/wallet.hpp>
#include <rai/common/util.hpp>
#include <rai/common/parameters.hpp>

rai::WalletServiceRunner::WalletServiceRunner(boost::asio::io_service& service)
    : service_(service), stopped_(false), thread_([this]() { this->Run(); })
{
}

rai::WalletServiceRunner::~WalletServiceRunner()
{
    Stop();
}
void rai::WalletServiceRunner::Notify()
{
    condition_.notify_all();
}

void rai::WalletServiceRunner::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_)
    {
        std::cout << "wait lock\n";
        condition_.wait_until(
            lock, std::chrono::steady_clock::now() + std::chrono::seconds(3));
        if (stopped_)
        {
            break;
        }
        lock.unlock();

        if (service_.stopped())
        {
            service_.reset();
        }

        try
        {
            std::cout << "service start\n";
            service_.run();
            std::cout << "service stop\n";
        }
        catch(const std::exception& e)
        {
            // log
            std::cout << "service exception: " << e.what() << std::endl;
        }
        
        lock.lock();
    }
}

void rai::WalletServiceRunner::Stop()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }
        stopped_ = true;
        service_.stop();
    }

    condition_.notify_all();
    Join();
}

void rai::WalletServiceRunner::Join()
{
    if (thread_.joinable())
    {
        thread_.join();
    }
}

rai::Wallet::Wallet(rai::Wallets& wallets)
    : wallets_(wallets),
      version_(rai::Wallet::VERSION_1),
      index_(0),
      selected_account_id_(0)
{
    rai::random_pool.GenerateBlock(salt_.bytes.data(), salt_.bytes.size());

    rai::RawKey key;
    rai::random_pool.GenerateBlock(key.data_.bytes.data(),
                                   key.data_.bytes.size());
    rai::RawKey password;
    fan_.Get(password);
    key_.Encrypt(key, password, salt_.owords[0]);

    rai::RawKey seed;
    rai::random_pool.GenerateBlock(seed.data_.bytes.data(),
                                   seed.data_.bytes.size());
    seed_.Encrypt(seed, key, salt_.owords[0]);

    rai::RawKey zero;
    zero.data_.Clear();
    check_.Encrypt(zero, key, salt_.owords[0]);

    selected_account_id_ = CreateAccount_();
}

rai::Wallet::Wallet(rai::Wallets& wallets, const rai::WalletInfo& info)
    : wallets_(wallets),
      version_(info.version_),
      index_(info.index_),
      selected_account_id_(info.selected_account_id_),
      salt_(info.salt_),
      key_(info.key_),
      seed_(info.seed_),
      check_(info.check_)
{
}

rai::Wallet::Wallet(rai::Wallets& wallets, const rai::RawKey& seed)
    : wallets_(wallets),
      version_(rai::Wallet::VERSION_1),
      index_(0),
      selected_account_id_(0)
{
    rai::random_pool.GenerateBlock(salt_.bytes.data(), salt_.bytes.size());

    rai::RawKey key;
    rai::random_pool.GenerateBlock(key.data_.bytes.data(),
                                   key.data_.bytes.size());
    rai::RawKey password;
    fan_.Get(password);
    key_.Encrypt(key, password, salt_.owords[0]);

    seed_.Encrypt(seed, key, salt_.owords[0]);

    rai::RawKey zero;
    zero.data_.Clear();
    check_.Encrypt(zero, key, salt_.owords[0]);

    selected_account_id_ = CreateAccount_();
}

rai::Account rai::Wallet::Account(uint32_t account_id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& i : accounts_)
    {
        if (i.first == account_id)
        {
            return i.second.public_key_;
        }
    }

    return rai::Account(0);
}

rai::ErrorCode rai::Wallet::CreateAccount(uint32_t& account_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    account_id = CreateAccount_();
    return rai::ErrorCode::SUCCESS;
}

std::vector<std::pair<uint32_t, std::pair<rai::Account, bool>>>
rai::Wallet::Accounts() const
{
    std::vector<std::pair<uint32_t, std::pair<rai::Account, bool>>> result;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& i : accounts_)
    {
        bool is_adhoc = i.second.index_ == std::numeric_limits<uint32_t>::max();
        result.emplace_back(i.first,
                            std::make_pair(i.second.public_key_, is_adhoc));
    }

    return result;
}

bool rai::Wallet::AttemptPassword(const std::string& password)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rai::RawKey kdf;
    rai::Kdf::DeriveKey(kdf, password, salt_);
    fan_.Set(kdf);
    return !ValidPassword_();
}

bool rai::Wallet::ValidPassword() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return ValidPassword_();
}

bool rai::Wallet::EmptyPassword() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return false;
    }
    rai::RawKey password;
    fan_.Get(password);
    return password.data_.IsZero();
}

bool rai::Wallet::IsMyAccount(const rai::Account& account) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& i: accounts_)
    {
        if (account == i.second.public_key_)
        {
            return true;
        }
    }
    return false;
}

rai::ErrorCode rai::Wallet::ChangePassword(const std::string& password)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    rai::RawKey kdf;
    rai::Kdf::DeriveKey(kdf, password, salt_);

    rai::RawKey key = Key_();
    fan_.Set(kdf);
    key_.Encrypt(key, kdf, salt_.owords[0]);

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallet::ImportAccount(const rai::KeyPair& key_pair,
                                          uint32_t& account_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    for (const auto& i : accounts_)
    {
        if (i.second.public_key_ == key_pair.public_key_)
        {
            return rai::ErrorCode::WALLET_ACCOUNT_EXISTS;
        }
    }

    rai::WalletAccountInfo info;
    info.index_ = std::numeric_limits<uint32_t>::max();
    info.private_key_.Encrypt(key_pair.private_key_, Key_(), salt_.owords[0]);
    info.public_key_ = key_pair.public_key_;
    account_id       = NewAccountId_();
    accounts_.emplace_back(account_id, info);

    return rai::ErrorCode::SUCCESS;
}

void rai::Wallet::Lock()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return;
    }

    rai::RawKey zero;
    zero.data_.Clear();
    fan_.Set(zero);
}

rai::ErrorCode rai::Wallet::PrivateKey(const rai::Account& account,
                                       rai::RawKey& private_key) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    bool error = PrivateKey_(account, private_key);
    IF_ERROR_RETURN(error, rai::ErrorCode::WALLET_ACCOUNT_GET);

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallet::Seed(rai::RawKey& seed) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    seed = Seed_();
    return rai::ErrorCode::SUCCESS;
}

bool rai::Wallet::Sign(const rai::Account& account,
                       const rai::uint256_union& message,
                       rai::Signature& signature) const
{
    rai::RawKey private_key;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ValidPassword_())
        {
            return true;
        }

        bool error = PrivateKey_(account, private_key);
        IF_ERROR_RETURN(error, true);
    }

    signature = rai::SignMessage(private_key, account, message);
    return false;
}

rai::ErrorCode rai::Wallet::Store(rai::Transaction& transaction,
                                  uint32_t wallet_id)
{
    std::lock_guard<std::mutex> lock(mutex_);

    bool error =
        wallets_.ledger_.WalletInfoPut(transaction, wallet_id, Info_());
    IF_ERROR_RETURN(error, rai::ErrorCode::WALLET_INFO_PUT);
    for (const auto& i : accounts_)
    {
        error = wallets_.ledger_.WalletAccountInfoPut(transaction, wallet_id,
                                                      i.first, i.second);
        IF_ERROR_RETURN(error, rai::ErrorCode::WALLET_ACCOUNT_INFO_PUT);
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallet::StoreInfo(rai::Transaction& transaction,
                                  uint32_t wallet_id)
{
    std::lock_guard<std::mutex> lock(mutex_);

    bool error =
        wallets_.ledger_.WalletInfoPut(transaction, wallet_id, Info_());
    IF_ERROR_RETURN(error, rai::ErrorCode::WALLET_INFO_PUT);

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallet::StoreAccount(rai::Transaction& transaction,
                                         uint32_t wallet_id,
                                         uint32_t account_id)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& i : accounts_)
    {
        if (i.first == account_id)
        {
            bool error = wallets_.ledger_.WalletAccountInfoPut(
                transaction, wallet_id, i.first, i.second);
            if (!error)
            {
                return rai::ErrorCode::SUCCESS;
            }
            return rai::ErrorCode::WALLET_ACCOUNT_INFO_PUT;
        }
    }

    return rai::ErrorCode::WALLET_ACCOUNT_GET;
}

bool rai::Wallet::SelectAccount(uint32_t account_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (account_id == selected_account_id_)
    {
        return true;
    }

    for (const auto& i : accounts_)
    {
        if (account_id == i.first)
        {
            selected_account_id_ = account_id;
            return false;
        }
    }

    return true;
}


rai::Account rai::Wallet::SelectedAccount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& i : accounts_)
    {
        if (selected_account_id_ == i.first)
        {
            return i.second.public_key_;
        }
    }
    return rai::Account(0);
}

rai::RawKey rai::Wallet::Decrypt_(const rai::uint256_union& encrypted) const
{
    rai::RawKey result;
    result.Decrypt(encrypted, Key_(), salt_.owords[0]);
    return result;
}

rai::RawKey rai::Wallet::Key_() const
{
    rai::RawKey password;
    fan_.Get(password);
    rai::RawKey key;
    key.Decrypt(key_, password, salt_.owords[0]);
    return key;
}

rai::RawKey rai::Wallet::Seed_() const
{
    rai::RawKey seed;
    seed.Decrypt(seed_, Key_(), salt_.owords[0]);
    return seed;
}

uint32_t rai::Wallet::CreateAccount_()
{
    blake2b_state hash;
    rai::RawKey private_key;
    rai::RawKey seed = Seed_();
    blake2b_init(&hash, private_key.data_.bytes.size());
    blake2b_update(&hash, seed.data_.bytes.data(), seed.data_.bytes.size());
    rai::uint256_union index(index_++);
    blake2b_update(&hash, reinterpret_cast<uint8_t*>(&index.dwords[7]),
                   sizeof(uint32_t));
    blake2b_final(&hash, private_key.data_.bytes.data(),
                  private_key.data_.bytes.size());
    rai::WalletAccountInfo info;
    info.index_ = index_ - 1;
    info.private_key_.Encrypt(private_key, Key_(), salt_.owords[0]);
    info.public_key_ = rai::GeneratePublicKey(private_key.data_);
    uint32_t account_id = NewAccountId_();
    accounts_.emplace_back(account_id, info);
    return account_id;
}

uint32_t rai::Wallet::NewAccountId_() const
{
    uint32_t result = 1;

    for (const auto& i : accounts_)
    {
        if (i.first >= result)
        {
            result = i.first + 1;
        }
    }

    return result;
}

bool rai::Wallet::ValidPassword_() const
{
    rai::RawKey zero;
    zero.data_.Clear();
    rai::uint256_union check;
    check.Encrypt(zero, Key_(), salt_.owords[0]);
    return check == check_;
}


rai::WalletInfo rai::Wallet::Info_() const
{
    rai::WalletInfo result;

    result.version_ = version_;
    result.index_ = index_;
    result.selected_account_id_ = selected_account_id_;
    result.salt_ = salt_;
    result.key_ = key_;
    result.seed_ = seed_;
    result.check_ = check_;

    return result;
}

bool rai::Wallet::PrivateKey_(const rai::Account& account,
                              rai::RawKey& private_key) const
{
    boost::optional<rai::uint256_union> encrypted_key(boost::none);
    for (const auto& i : accounts_)
    {
        if (i.second.public_key_ == account)
        {
            encrypted_key = i.second.private_key_;
        }
    }

    if (!encrypted_key)
    {
        return true;
    }
    private_key = Decrypt_(*encrypted_key);
    return false;
}

rai::Wallets::Wallets(rai::ErrorCode& error_code,
                      boost::asio::io_service& service, rai::Alarm& alarm,
                      const boost::filesystem::path& data_path,
                      const rai::WalletConfig& config,
                      rai::BlockType block_type, const rai::RawKey& seed)
    : service_(service),
      alarm_(alarm),
      config_(config),
      store_(error_code, data_path / "wallet_data.ldb"),
      ledger_(error_code, store_, false),
      websocket_(*this, config.server_.host_, config.server_.port_,
                 config.server_.path_),
      service_runner_(service),
      block_type_(block_type),
      stopped_(false),
      selected_wallet_id_(0),
      thread_([this]() { this->Run(); })
{
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    if (config_.preconfigured_reps_.empty())
    {
        error_code = rai::ErrorCode::JSON_CONFIG_WALLET_PRECONFIGURED_REP;
        return;
    }

    rai::Transaction transaction(error_code, ledger_, true);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);
    std::vector<std::pair<uint32_t, rai::WalletInfo>> infos;
    bool error = ledger_.WalletInfoGetAll(transaction, infos);
    if (!error && !infos.empty())
    {
        for (const auto& info : infos)
        {
            auto wallet = std::make_shared<rai::Wallet>(*this, info.second);
            error = ledger_.WalletAccountInfoGetAll(transaction, info.first,
                                                    wallet->accounts_);
            if (error)
            {
                throw std::runtime_error("WalletAccountInfoGetAll error");
            }
            wallets_.emplace_back(info.first, wallet);
        }
    }
    else
    {
        std::shared_ptr<rai::Wallet> wallet(nullptr);
        if (seed.data_.IsZero())
        {
            wallet = std::make_shared<rai::Wallet>(*this);
        }
        else
        {
            wallet = std::make_shared<rai::Wallet>(*this, seed);
        }
        uint32_t wallet_id = 1;
        error_code = wallet->Store(transaction, wallet_id);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            transaction.Abort();
            throw std::runtime_error("Wallet store error");
            return;
        }
        wallets_.emplace_back(wallet_id, wallet);
    }

    error = ledger_.SelectedWalletIdGet(transaction, selected_wallet_id_);
    if (error)
    {
        selected_wallet_id_ = wallets_[0].first;
        error = ledger_.SelectedWalletIdPut(transaction, selected_wallet_id_);
        if (error)
        {
            throw std::runtime_error("SelectedWalletIdPut error");
            return;
        }
    }

    InitReceived_(transaction);
}

std::shared_ptr<rai::Wallets> rai::Wallets::Shared()
{
    return shared_from_this();
}

void rai::Wallets::AccountBalance(const rai::Account& account,
                                  rai::Amount& balance)
{
    balance.Clear();
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN_VOID(error);

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        //log
        return;
    }
    balance = head->Balance();
}

void rai::Wallets::AccountBalanceConfirmed(const rai::Account& account,
                                           rai::Amount& confirmed)
{
    confirmed.Clear();

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN_VOID(error);

    if (info.confirmed_height_ != rai::Block::INVALID_HEIGHT)
    {
        std::shared_ptr<rai::Block> confirmed_block(nullptr);
        error = ledger_.BlockGet(transaction, account, info.confirmed_height_,
                                 confirmed_block);
        if (error || confirmed_block == nullptr)
        {
            // log
            return;
        }
        confirmed = confirmed_block->Balance();
    }
}

void rai::Wallets::AccountBalanceAll(const rai::Account& account,
                                     rai::Amount& balance,
                                     rai::Amount& confirmed,
                                     rai::Amount& receivable)
{
    balance.Clear();
    confirmed.Clear();
    receivable.Clear();

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    rai::Amount sum(0);
    rai::Iterator i = ledger_.ReceivableInfoLowerBound(transaction, account);
    rai::Iterator n = ledger_.ReceivableInfoUpperBound(transaction, account);
    for (; i != n; ++i)
    {
        rai::Account account_l;
        rai::BlockHash hash;
        rai::ReceivableInfo info;
        bool error = ledger_.ReceivableInfoGet(i, account_l, hash, info);
        if (error)
        {
            // log
            return;
        }
        sum += info.amount_;
    }
    receivable = sum;

    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN_VOID(error);

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        //log
        return;
    }
    balance = head->Balance();

    if (info.confirmed_height_ == info.head_height_)
    {
        confirmed = balance;
    }
    else if (info.confirmed_height_ != rai::Block::INVALID_HEIGHT)
    {
        std::shared_ptr<rai::Block> confirmed_block(nullptr);
        error = ledger_.BlockGet(transaction, account, info.confirmed_height_,
                                 confirmed_block);
        if (error || confirmed_block == nullptr)
        {
            //log
            return;
        }
        confirmed = confirmed_block->Balance();
    }
    else
    {
        /* do nothing */
    }
}

bool rai::Wallets::AccountRepresentative(const rai::Account& account,
                                         rai::Account& rep)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN(error, true);

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        return true;
    }

    if (!head->HasRepresentative())
    {
        return true;
    }

    rep = head->Representative();
    return false;
}

void rai::Wallets::AccountTransactionsLimit(const rai::Account& account,
                                            uint32_t& total, uint32_t& used)
{
    total = 0;
    used = 0;
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN_VOID(error);

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        return;
    }

    total = head->Credit() * rai::TRANSACTIONS_PER_CREDIT;
    if (rai::SameDay(head->Timestamp(), rai::CurrentTimestamp()))
    {
        used = head->Counter();
    }
    else
    {
        used = 0;
    }
}

void rai::Wallets::AccountInfoQuery(const rai::Account& account)
{
    rai::Ptree ptree;
    ptree.put("action", "account_info");
    ptree.put("account", account.StringAccount());
    websocket_.Send(ptree);
}

rai::ErrorCode rai::Wallets::AccountChange(
    const rai::Account& rep, const rai::AccountActionCallback& callback)
{
    auto wallet = SelectedWallet();
    if (wallet == nullptr)
    {
        return rai::ErrorCode::WALLET_GET;
    }

    if (!wallet->ValidPassword())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    rai::Account account = wallet->SelectedAccount();
    if (account.IsZero())
    {
        return rai::ErrorCode::WALLET_ACCOUNT_GET;
    }

    std::weak_ptr<rai::Wallets> this_w(Shared());
    QueueAction(
        rai::WalletActionPri::HIGH, [this_w, wallet, account, rep, callback]() {
            if (auto this_s = this_w.lock())
            {
                this_s->ProcessAccountChange(wallet, account, rep, callback);
            }
        });
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::AccountCredit(
    uint16_t credit, const rai::AccountActionCallback& callback)
{
    auto wallet = SelectedWallet();
    if (wallet == nullptr)
    {
        return rai::ErrorCode::WALLET_GET;
    }

    if (!wallet->ValidPassword())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    rai::Account account = wallet->SelectedAccount();
    if (account.IsZero())
    {
        return rai::ErrorCode::WALLET_ACCOUNT_GET;
    }

    std::weak_ptr<rai::Wallets> this_w(Shared());
    QueueAction(rai::WalletActionPri::HIGH, [this_w, wallet, account, credit,
                                             callback]() {
        if (auto this_s = this_w.lock())
        {
            this_s->ProcessAccountCredit(wallet, account, credit, callback);
        }
    });
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::AccountSend(
    const rai::Account& destination, const rai::Amount& amount,
    const rai::AccountActionCallback& callback)
{
    auto wallet = SelectedWallet();
    if (wallet == nullptr)
    {
        return rai::ErrorCode::WALLET_GET;
    }

    if (!wallet->ValidPassword())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    rai::Account account = wallet->SelectedAccount();
    if (account.IsZero())
    {
        return rai::ErrorCode::WALLET_ACCOUNT_GET;
    }

    std::weak_ptr<rai::Wallets> this_w(Shared());
    QueueAction(rai::WalletActionPri::HIGH,
                [this_w, wallet, account, destination, amount, callback]() {
                    if (auto this_s = this_w.lock())
                    {
                        this_s->ProcessAccountSend(wallet, account, destination,
                                                   amount, callback);
                    }
                });
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::AccountReceive(
    const rai::Account& account, const rai::BlockHash& hash,
    const rai::AccountActionCallback& callback)
{
    auto wallet = SelectedWallet();
    if (wallet == nullptr)
    {
        return rai::ErrorCode::WALLET_GET;
    }

    if (!wallet->ValidPassword())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    if (account != wallet->SelectedAccount())
    {
        return rai::ErrorCode::WALLET_NOT_SELECTED_ACCOUNT;
    }

    std::weak_ptr<rai::Wallets> this_w(Shared());
    QueueAction(rai::WalletActionPri::HIGH,
                [this_w, wallet, account, hash, callback]() {
                    if (auto this_s = this_w.lock())
                    {
                        this_s->ProcessAccountReceive(wallet, account, hash,
                                                      callback);
                    }
                });
    return rai::ErrorCode::SUCCESS;
}

void rai::Wallets::BlockQuery(const rai::Account& account, uint64_t height,
                              const rai::BlockHash& previous)
{
    rai::Ptree ptree;
    ptree.put("action", "block_query");
    ptree.put("account", account.StringAccount());
    ptree.put("height", std::to_string(height));
    ptree.put("previous", previous.StringHex());
    websocket_.Send(ptree);
}

void rai::Wallets::BlockPublish(const std::shared_ptr<rai::Block>& block)
{
    rai::Ptree ptree;
    ptree.put("action", "block_publish");
    rai::Ptree block_ptree;
    block->SerializeJson(block_ptree);
    ptree.put_child("block", block_ptree);
    websocket_.Send(ptree);
}

void rai::Wallets::ConnectToServer()
{
    websocket_.Run();
}

rai::ErrorCode rai::Wallets::ChangePassword(const std::string& password)
{
    bool empty_password = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto wallet = Wallet_(selected_wallet_id_);
        if (wallet == nullptr)
        {
            return rai::ErrorCode::WALLET_GET;
        }
        empty_password = wallet->EmptyPassword();

        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            return error_code;
        }

        error_code = wallet->ChangePassword(password);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = wallet->StoreInfo(transaction, selected_wallet_id_);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            transaction.Abort();
            return error_code;
        }
    }

    if (empty_password && wallet_password_set_observer_)
    {
        wallet_password_set_observer_();
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::CreateAccount()
{
    uint32_t account_id = 0;
    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wallet = Wallet_(selected_wallet_id_);
        if (wallet == nullptr)
        {
            // log
            return rai::ErrorCode::WALLET_GET;
        }

        
        rai::ErrorCode error_code = wallet->CreateAccount(account_id);
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return error_code;
        }
        error_code =
            wallet->StoreAccount(transaction, selected_wallet_id_, account_id);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            transaction.Abort();
            return error_code;
        }
        error_code = wallet->StoreInfo(transaction, selected_wallet_id_);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            transaction.Abort();
            return error_code;
        }
    }

    Subscribe(wallet, account_id);
    Sync(wallet->Account(account_id));
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::CreateWallet()
{
    auto wallet = std::make_shared<rai::Wallet>(*this);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t wallet_id = NewWalletId_();
        if (wallet_id == 0)
        {
            return rai::ErrorCode::GENERIC;
        }

        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            return error_code;
        }
        error_code = wallet->Store(transaction, wallet_id);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            transaction.Abort();
            return error_code;
        }
        wallets_.emplace_back(wallet_id, wallet);
    }

    Subscribe(wallet);
    Sync(wallet);
    return rai::ErrorCode::SUCCESS;
}

bool rai::Wallets::EnterPassword(const std::string& password)
{
    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wallet = Wallet_(selected_wallet_id_);
        if (wallet == nullptr)
        {
            return true;
        }
    }

    bool error = wallet->AttemptPassword(password);
    IF_ERROR_RETURN(error, true);

    if (wallet_locked_observer_)
    {
        wallet_locked_observer_(false);
    }

    return false;
}

rai::ErrorCode rai::Wallets::ImportAccount(const rai::KeyPair& key_pair)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, true);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return error_code;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto wallet = Wallet_(selected_wallet_id_);
    uint32_t account_id = 0;
    error_code = wallet->ImportAccount(key_pair, account_id);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        wallet->StoreAccount(transaction, selected_wallet_id_, account_id);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::ImportWallet(const rai::RawKey& seed,
                                          uint32_t& wallet_id)
{
    auto wallet = std::make_shared<rai::Wallet>(*this, seed);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rai::Account account = wallet->SelectedAccount();
        for (const auto& i : wallets_)
        {
            if (i.second->IsMyAccount(account))
            {
                wallet_id = i.first;
                return rai::ErrorCode::WALLET_EXISTS;
            }
        }

        wallet_id = NewWalletId_();
        if (wallet_id == 0)
        {
            return rai::ErrorCode::GENERIC;
        }

        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            return error_code;
        }
        error_code = wallet->Store(transaction, wallet_id);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            transaction.Abort();
            return error_code;
        }
        wallets_.emplace_back(wallet_id, wallet);
    }

    Subscribe(wallet);
    Sync(wallet);
    return rai::ErrorCode::SUCCESS;
}

bool rai::Wallets::IsMyAccount(const rai::Account& account) const
{
    std::unique_lock<std::mutex> lock(mutex_);
    for (const auto& i : wallets_)
    {
        if (i.second->IsMyAccount(account))
        {
            return true;
        }
    }
    return false;
}

void rai::Wallets::LockWallet(uint32_t wallet_id)
{
    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wallet = Wallet_(wallet_id);
        if (wallet == nullptr)
        {
            return;
        }
    }

    wallet->Lock();
    if (wallet_locked_observer_)
    {
        wallet_locked_observer_(true);
    }
}

void rai::Wallets::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_)
    {
        if (actions_.empty())
        {
            condition_.wait(lock);
            continue;
        }

        auto front = actions_.begin();
        auto action(std::move(front->second));
        actions_.erase(front);

        lock.unlock();
        action();
        lock.lock();
    }
}

void rai::Wallets::Ongoing(const std::function<void()>& process,
                           const std::chrono::seconds& delay)
{
    process();
    std::weak_ptr<rai::Wallets> wallets(Shared());
    alarm_.Add(std::chrono::steady_clock::now() + delay,
               [wallets, process, delay]() {
                   auto wallets_l = wallets.lock();
                   if (wallets_l)
                   {
                       wallets_l->Ongoing(process, delay);
                   }
               });
}

void rai::Wallets::ProcessAccountInfo(const rai::Account& account,
                                      const rai::AccountInfo& info)
{
    if (!IsMyAccount(account))
    {
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    rai::AccountInfo info_l;
    bool error = ledger_.AccountInfoGet(transaction, account, info_l);
    if (error || !info_l.Valid())
    {
        return;
    }

    if (info_l.head_height_ <= info.head_height_)
    {
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::BlockHash successor;
    error = ledger_.BlockGet(transaction, info.head_, block, successor);
    if (error || block == nullptr)
    {
        return;
    }

    while (block->Height() < info_l.head_height_)
    {
        error = ledger_.BlockGet(transaction, successor, block, successor);
        if (error)
        {
            // log
            std::cout << "ProcessAccountInfo::BlockGet error(account="
                      << account.StringAccount()
                      << ",hash=" << successor.StringHex() << ")\n";
            return;
        }
        BlockPublish(block);
    }
}

void rai::Wallets::ProcessAccountChange(
    const std::shared_ptr<rai::Wallet>& wallet, const rai::Account& account,
    const rai::Account& rep, const rai::AccountActionCallback& callback)
{
    std::shared_ptr<rai::Block> block(nullptr);
    if (!wallet->ValidPassword())
    {
        callback(rai::ErrorCode::WALLET_LOCKED, block);
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    rai::AccountInfo info;
    bool error =
        ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        callback(rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET, block);
        return;
    }

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        callback(rai::ErrorCode::LEDGER_BLOCK_GET, block);
        return;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::CHANGE;
    uint16_t credit = head->Credit();

    uint64_t now = rai::CurrentTimestamp();
    uint64_t timestamp = now > head->Timestamp() ? now : head->Timestamp();
    if (timestamp > now + 60)
    {
        callback(rai::ErrorCode::BLOCK_TIMESTAMP, block);
        return;
    }
    if (info.forks_ > rai::MaxAllowedForks(timestamp))
    {
        callback(rai::ErrorCode::ACCOUNT_LIMITED, block);
        return;
    }

    uint32_t counter =
        rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() + 1 : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        callback(rai::ErrorCode::ACCOUNT_ACTION_CREDIT, block);
        return;
    }
    uint64_t height = head->Height() + 1;
    rai::BlockHash previous = info.head_;
    rai::Amount balance = head->Balance();
    rai::uint256_union link(0);
    rai::RawKey private_key;
    error_code = wallet->PrivateKey(account, private_key);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    if (info.type_ == rai::BlockType::TX_BLOCK)
    {
        block = std::make_shared<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account, previous, rep,
            balance, link, 0, std::vector<uint8_t>(), private_key, account);
    }
    else if (info.type_ == rai::BlockType::AD_BLOCK)
    {
        block = std::make_shared<rai::AdBlock>(
            opcode, credit, counter, timestamp, height, account, previous, rep,
            balance, link, private_key, account);
    }
    else
    {
        callback(rai::ErrorCode::BLOCK_TYPE, block);
        return;
    }

    ProcessBlock(block, false);
    BlockPublish(block);
    callback(rai::ErrorCode::SUCCESS, block);
}

void rai::Wallets::ProcessAccountCredit(
    const std::shared_ptr<rai::Wallet>& wallet, const rai::Account& account,
    uint16_t credit_inc, const rai::AccountActionCallback& callback)
{
    std::shared_ptr<rai::Block> block(nullptr);
    if (!wallet->ValidPassword())
    {
        callback(rai::ErrorCode::WALLET_LOCKED, block);
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    rai::AccountInfo info;
    bool error =
        ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        callback(rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET, block);
        return;
    }

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        callback(rai::ErrorCode::LEDGER_BLOCK_GET, block);
        return;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::CREDIT;

    uint16_t credit = head->Credit() + credit_inc;
    if (credit_inc >= rai::MAX_ACCOUNT_CREDIT
        || credit > rai::MAX_ACCOUNT_CREDIT)
    {
        callback(rai::ErrorCode::ACCOUNT_MAX_CREDIT, block);
        return;
    }

    uint64_t now = rai::CurrentTimestamp();
    uint64_t timestamp = now > head->Timestamp() ? now : head->Timestamp();
    if (timestamp > now + 60)
    {
        callback(rai::ErrorCode::BLOCK_TIMESTAMP, block);
        return;
    }
    if (info.forks_ > rai::MaxAllowedForks(timestamp))
    {
        callback(rai::ErrorCode::ACCOUNT_LIMITED, block);
        return;
    }

    uint32_t counter =
        rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() + 1 : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        callback(rai::ErrorCode::ACCOUNT_ACTION_CREDIT, block);
        return;
    }

    uint64_t height = head->Height() + 1;
    rai::BlockHash previous = info.head_;

    rai::Amount cost(rai::CreditPrice(timestamp).Number() * credit_inc);
    if (cost > head->Balance())
    {
        callback(rai::ErrorCode::ACCOUNT_ACTION_BALANCE, block);
        return;
    }
    rai::Amount balance = head->Balance() - cost;

    rai::uint256_union link(0);
    rai::RawKey private_key;
    error_code = wallet->PrivateKey(account, private_key);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    if (info.type_ == rai::BlockType::TX_BLOCK)
    {
        block = std::make_shared<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account, previous,
            head->Representative(), balance, link, 0, std::vector<uint8_t>(),
            private_key, account);
    }
    else
    {
        callback(rai::ErrorCode::BLOCK_TYPE, block);
        return;
    }
    
    ProcessBlock(block, false);
    BlockPublish(block);
    callback(rai::ErrorCode::SUCCESS, block);
}

void rai::Wallets::ProcessAccountSend(
    const std::shared_ptr<rai::Wallet>& wallet, const rai::Account& account,
    const rai::Account& destination, const rai::Amount& amount,
    const rai::AccountActionCallback& callback)
{
    std::shared_ptr<rai::Block> block(nullptr);
    if (!wallet->ValidPassword())
    {
        callback(rai::ErrorCode::WALLET_LOCKED, block);
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    rai::AccountInfo info;
    bool error =
        ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        callback(rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET, block);
        return;
    }

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        callback(rai::ErrorCode::LEDGER_BLOCK_GET, block);
        return;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::SEND;
    uint16_t credit = head->Credit();

    uint64_t now = rai::CurrentTimestamp();
    uint64_t timestamp = now > head->Timestamp() ? now : head->Timestamp();
    if (timestamp > now + 60)
    {
        callback(rai::ErrorCode::BLOCK_TIMESTAMP, block);
        return;
    }
    if (info.forks_ > rai::MaxAllowedForks(timestamp))
    {
        callback(rai::ErrorCode::ACCOUNT_LIMITED, block);
        return;
    }
    
    uint32_t counter =
        rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() + 1 : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        callback(rai::ErrorCode::ACCOUNT_ACTION_CREDIT, block);
        return;
    }
    uint64_t height = head->Height() + 1;
    rai::BlockHash previous = info.head_;
    if (head->Balance() < amount)
    {
        callback(rai::ErrorCode::ACCOUNT_ACTION_BALANCE, block);
        return;
    }
    rai::Amount balance = head->Balance() - amount;
    rai::uint256_union link = destination;
    rai::RawKey private_key;
    error_code = wallet->PrivateKey(account, private_key);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    if (info.type_ == rai::BlockType::TX_BLOCK)
    {
        block = std::make_shared<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account, previous,
            head->Representative(), balance, link, 0, std::vector<uint8_t>(),
            private_key, account);
    }
    else
    {
        callback(rai::ErrorCode::BLOCK_TYPE, block);
        return;
    }
    
    ProcessBlock(block, false);
    BlockPublish(block);
    callback(rai::ErrorCode::SUCCESS, block);
}

void rai::Wallets::ProcessAccountReceive(
    const std::shared_ptr<rai::Wallet>& wallet, const rai::Account& account,
    const rai::BlockHash& hash, const rai::AccountActionCallback& callback)
{
    std::shared_ptr<rai::Block> block(nullptr);
    if (!wallet->ValidPassword())
    {
        callback(rai::ErrorCode::WALLET_LOCKED, block);
        return;
    }


    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    rai::ReceivableInfo receivable_info;
    bool error =
        ledger_.ReceivableInfoGet(transaction, account, hash, receivable_info);
    if (error)
    {
        callback(rai::ErrorCode::LEDGER_RECEIVABLE_INFO_GET, block);
        return;
    }

    rai::AccountInfo account_info;
    error =
        ledger_.AccountInfoGet(transaction, account, account_info);
    if (error || !account_info.Valid())
    {
        rai::BlockOpcode opcode = rai::BlockOpcode::RECEIVE;
        uint16_t credit = 1;
        uint64_t now = rai::CurrentTimestamp();
        uint64_t timestamp = now <= receivable_info.timestamp_
                                 ? receivable_info.timestamp_ + 1
                                 : now;
        if (timestamp > now + 60)
        {
            callback(rai::ErrorCode::ACCOUNT_ACTION_TOO_QUICKLY, block);
            return;
        }
        uint32_t counter = 1;
        uint64_t height = 0;
        rai::BlockHash previous(0);
        if (receivable_info.amount_ < rai::CreditPrice(timestamp))
        {
            callback(rai::ErrorCode::WALLET_RECEIVABLE_LESS_THAN_CREDIT, block);
            return;
        }
        rai::Amount balance =
            receivable_info.amount_ - rai::CreditPrice(timestamp);
        rai::uint256_union link = hash;
        rai::Account rep = RandomRepresentative();
        rai::RawKey private_key;
        error_code = wallet->PrivateKey(account, private_key);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            callback(error_code, block);
            return;
        }

        if (block_type_ == rai::BlockType::TX_BLOCK)
        {
            block = std::make_shared<rai::TxBlock>(
                opcode, credit, counter, timestamp, height, account, previous,
                rep, balance, link, 0, std::vector<uint8_t>(),
                private_key, account);
        }
        else if (block_type_ == rai::BlockType::AD_BLOCK)
        {
            block = std::make_shared<rai::AdBlock>(
                opcode, credit, counter, timestamp, height, account, previous,
                rep, balance, link, private_key, account);
        }
        else
        {
            callback(rai::ErrorCode::BLOCK_TYPE, block);
            return;
        }
    }
    else
    {
        std::shared_ptr<rai::Block> head(nullptr);
        error = ledger_.BlockGet(transaction, account_info.head_, head);
        if (error || head == nullptr)
        {
            callback(rai::ErrorCode::LEDGER_BLOCK_GET, block);
            return;
        }

        rai::BlockOpcode opcode = rai::BlockOpcode::RECEIVE;
        uint16_t credit = head->Credit();

        uint64_t now = rai::CurrentTimestamp();
        uint64_t timestamp = now > head->Timestamp() ? now : head->Timestamp();
        if (timestamp > now + 60)
        {
            callback(rai::ErrorCode::BLOCK_TIMESTAMP, block);
            return;
        }
        if (account_info.forks_ > rai::MaxAllowedForks(timestamp))
        {
            callback(rai::ErrorCode::ACCOUNT_LIMITED, block);
            return;
        }

        uint32_t counter =
            rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() + 1 : 1;
        if (counter
            > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
        {
            callback(rai::ErrorCode::ACCOUNT_ACTION_CREDIT, block);
            return;
        }
        uint64_t height = head->Height() + 1;
        rai::BlockHash previous = account_info.head_;
        rai::Amount balance = head->Balance() + receivable_info.amount_;
        rai::uint256_union link = hash;
        rai::RawKey private_key;
        error_code = wallet->PrivateKey(account, private_key);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            callback(error_code, block);
            return;
        }

        if (account_info.type_ == rai::BlockType::TX_BLOCK)
        {
            block = std::make_shared<rai::TxBlock>(
                opcode, credit, counter, timestamp, height, account, previous,
                head->Representative(), balance, link, 0, std::vector<uint8_t>(),
                private_key, account);
        }
        else if (account_info.type_ == rai::BlockType::AD_BLOCK)
        {
            block = std::make_shared<rai::AdBlock>(
                opcode, credit, counter, timestamp, height, account, previous,
                head->Representative(), balance, link, private_key, account);
        }
        else
        {
            callback(rai::ErrorCode::BLOCK_TYPE, block);
            return;
        }
    }

    ProcessBlock(block, false);
    BlockPublish(block);
    callback(rai::ErrorCode::SUCCESS, block);
}

void rai::Wallets::ProcessBlock(const std::shared_ptr<rai::Block>& block,
                                 bool confirmed)
{
    if (!IsMyAccount(block->Account()))
    {
        return;
    }

    do
    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return;
        }

        rai::AccountInfo info;
        bool error =
            ledger_.AccountInfoGet(transaction, block->Account(), info);
        bool account_exist = !error && info.Valid();
        if (!account_exist)
        {
            if (block->Height() != 0)
            {
                return;
            }
            rai::AccountInfo new_info(block->Type(), block->Hash());
            if (confirmed)
            {
                new_info.confirmed_height_ = 0;
            }
            error =
                ledger_.AccountInfoPut(transaction, block->Account(), new_info);
            if (error)
            {
                // log
                return;
            }
            error = ledger_.BlockPut(transaction, block->Hash(), *block);
            if (error)
            {
                transaction.Abort();
                // log
            }

            if (block->Opcode() == rai::BlockOpcode::RECEIVE)
            {
                ledger_.ReceivableInfoDel(transaction, block->Account(),
                                          block->Link());
                ReceivedAdd(block->Link());
            }
            break;
        }

        if (block->Height() > info.head_height_ + 1)
        {
            return;
        }
        else if (block->Height() == info.head_height_ + 1)
        {
            if (block->Previous() != info.head_)
            {
                if (!confirmed || info.confirmed_height_ != info.head_height_)
                {
                    return;
                }
                // log
                info.confirmed_height_ = info.head_height_ - 1;
                if (info.head_height_ == 0)
                {
                    info.confirmed_height_ = rai::Block::INVALID_HEIGHT;
                }
                error =
                    ledger_.AccountInfoPut(transaction, block->Account(), info);
                if (error)
                {
                    // log
                    return;
                }
            }
            else
            {
                info.head_height_ = block->Height();
                info.head_        = block->Hash();
                if (confirmed)
                {
                    info.confirmed_height_ = block->Height();
                }
                error =
                    ledger_.AccountInfoPut(transaction, block->Account(), info);
                if (error)
                {
                    // log
                    transaction.Abort();
                    return;
                }
                error = ledger_.BlockPut(transaction, block->Hash(), *block);
                if (error)
                {
                    transaction.Abort();
                    // log
                    return;
                }
                error = ledger_.BlockSuccessorSet(
                    transaction, block->Previous(), block->Hash());
                if (error)
                {
                    transaction.Abort();
                    // log
                    return;
                }

                if (block->Opcode() == rai::BlockOpcode::RECEIVE)
                {
                    ledger_.ReceivableInfoDel(transaction, block->Account(),
                                              block->Link());
                    ReceivedAdd(block->Link());
                }
            }
        }
        else
        {
            if (!confirmed)
            {
                return;
            }

            if (ledger_.BlockExists(transaction, block->Hash()))
            {
                if (info.confirmed_height_ != rai::Block::INVALID_HEIGHT
                    && info.confirmed_height_ >= block->Height())
                {
                    return;
                }
                info.confirmed_height_ = block->Height();
                error =
                    ledger_.AccountInfoPut(transaction, block->Account(), info);
                if (error)
                {
                    // log
                    return;
                }
            }
            else
            {
                error = Rollback_(transaction, block->Account());
                if (error)
                {
                    transaction.Abort();
                    return;
                }
                QueueBlock(block, confirmed);
            }
        }
    } while (0);

    if (block_observer_)
    {
        block_observer_(block, false);
    }
}

void rai::Wallets::ProcessBlockRollback(
    const std::shared_ptr<rai::Block>& block)
{
    if (!IsMyAccount(block->Account()))
    {
        return;
    }

    bool callback = false;
    do
    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return;
        }

        bool error = false;
        while (ledger_.BlockExists(transaction, block->Hash()))
        {
            callback = true;
            error = Rollback_(transaction, block->Account());
            if (error)
            {
                //log
                std::cout << "Rollback error\n";
                transaction.Abort();
                return;
            }
        }
    } while (0);

    if (callback && block_observer_)
    {
        block_observer_(block, true);
    }
}

void rai::Wallets::ProcessReceivableInfo(const rai::Account& account,
                                         const rai::BlockHash& hash,
                                         const rai::ReceivableInfo& info)
{
    if (!IsMyAccount(account))
    {
        return;
    }

    if (info.timestamp_ > rai::CurrentTimestamp() + 30)
    {
        return;
    }

    if (Received(hash))
    {
        return;
    }

    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return;
        }

        bool error = ledger_.ReceivableInfoPut(transaction, account, hash, info);
        if (error)
        {
            // log
            return;
        }
    }

    if (receivable_)
    {
        receivable_(account);
    }
}

void rai::Wallets::QueueAccountInfo(const rai::Account& account,
                                    const rai::AccountInfo& info)
{
    std::weak_ptr<rai::Wallets> wallets(Shared());
    QueueAction(rai::WalletActionPri::URGENT, [wallets, account, info]() {
        if (auto wallets_s = wallets.lock())
        {
            wallets_s->ProcessAccountInfo(account, info);
        }
    });
}

void rai::Wallets::QueueAction(rai::WalletActionPri pri,
                               const std::function<void()>& action)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        actions_.insert(std::make_pair(pri, action));
    }
    condition_.notify_all();
}

void rai::Wallets::QueueBlock(const std::shared_ptr<rai::Block>& block,
                              bool confirmed)
{
    std::weak_ptr<rai::Wallets> wallets(Shared());
    QueueAction(rai::WalletActionPri::URGENT, [wallets, block, confirmed]() {
        if (auto wallets_s = wallets.lock())
        {
            wallets_s->ProcessBlock(block, confirmed);
        }
    });
}

void rai::Wallets::QueueBlockRollback(const std::shared_ptr<rai::Block>& block)
{
    std::weak_ptr<rai::Wallets> wallets(Shared());
    QueueAction(rai::WalletActionPri::URGENT, [wallets, block]() {
        if (auto wallets_s = wallets.lock())
        {
            wallets_s->ProcessBlockRollback(block);
        }
    });

}

void rai::Wallets::QueueReceivable(const rai::Account& account,
                                   const rai::BlockHash& hash,
                                   const rai::Amount& amount,
                                   const rai::Account& source,
                                   uint64_t timestamp)
{
    rai::ReceivableInfo info(source, amount, timestamp);
    std::weak_ptr<rai::Wallets> wallets(Shared());
    QueueAction(rai::WalletActionPri::URGENT, [wallets, account, hash, info]() {
        if (auto wallets_s = wallets.lock())
        {
            wallets_s->ProcessReceivableInfo(account, hash, info);
        }
    });
}

rai::Account rai::Wallets::RandomRepresentative() const
{
    size_t size = config_.preconfigured_reps_.size();
    if (size == 0)
    {
        throw std::runtime_error("No preconfigured representatives");
        return rai::Account(0);
    }

    if (config_.preconfigured_reps_.size() == 1)
    {
        return config_.preconfigured_reps_[0];
    }

    auto index = rai::random_pool.GenerateWord32(0, size - 1);
    return config_.preconfigured_reps_[index];
}


void rai::Wallets::ReceivablesQuery(const rai::Account& account)
{
    rai::Ptree ptree;
    ptree.put("action", "receivables");
    ptree.put("account", account.StringAccount());
    ptree.put("type", "confirmed");
    ptree.put("count", std::to_string(1000));

    websocket_.Send(ptree);
}

void rai::Wallets::ReceiveAccountInfoQueryAck(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto block_o = message->get_child_optional("head_block");
    if (!block_o)
    {
        // log
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block = rai::DeserializeBlockJson(error_code, *block_o);
    if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
    {
        //log
        return;
    }

    rai::AccountInfo info;
    info.type_ = block->Type();
    info.head_ = block->Hash();
    info.head_height_ = block->Height();

    QueueAccountInfo(block->Account(), info);
}

void rai::Wallets::ReceiveBlockAppendNotify(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto block_o = message->get_child_optional("block");
    if (!block_o)
    {
        // log
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block = rai::DeserializeBlockJson(error_code, *block_o);
    if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
    {
        //log
        return;
    }

    QueueBlock(block, false);
}

void rai::Wallets::ReceiveBlockConfirmNotify(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto block_o = message->get_child_optional("block");
    if (!block_o)
    {
        // log
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block = rai::DeserializeBlockJson(error_code, *block_o);
    if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
    {
        // log
        return;
    }

    QueueBlock(block, true);
}

void rai::Wallets::ReceiveBlockRollbackNotify(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto block_o = message->get_child_optional("block");
    if (!block_o)
    {
        // log
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block = rai::DeserializeBlockJson(error_code, *block_o);
    if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
    {
        // log
        return;
    }

    QueueBlockRollback(block);
}

void rai::Wallets::ReceiveBlockQueryAck(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto status_o = message->get_optional<std::string>("status");
    if (!status_o)
    {
        //log
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    auto block_o = message->get_child_optional("block");
    if (block_o)
    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        block = rai::DeserializeBlockJson(error_code, *block_o);
        if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
        {
            //log
            return;
        }
    }

    bool confirmed;
    auto confirmed_o = message->get_optional<std::string>("confirmed");
    if (confirmed_o)
    {
        if (*confirmed_o != "true" && *confirmed_o != "false")
        {
            //log
            return;
        }
        confirmed = *confirmed_o == "true" ? true : false;
    }

    if (*status_o == "success")
    {
        if (!block)
        {
            // log
            return;
        }
        if (!confirmed_o)
        {
            //log
            return;
        }
        QueueBlock(block, confirmed);
    }
    else if (*status_o == "fork")
    {
        if (!block)
        {
            // log
            return;
        }
        if (!confirmed_o)
        {
            //log
            return;
        }
        QueueBlock(block, confirmed);
        SyncForks(block->Account());
    }
    else if (*status_o == "miss")
    {

    }
    else if (*status_o == "pruned")
    {
        //log
    }
    else
    {
        //log
    }
    
}

void rai::Wallets::ReceiveReceivablesQueryAck(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto account_o = message->get_optional<std::string>("account");
    if (!account_o)
    {
        // log
        std::cout << "ReceiveReceivablesQueryAck::get account error\n";
        return;
    }
    rai::Account account;
    bool error = account.DecodeAccount(*account_o);
    if (error)
    {
        // log
        std::cout << "ReceiveReceivablesQueryAck::DecodeAccount error\n";
        return;
    }

    auto receivables_o = message->get_child_optional("receivables");
    if (!receivables_o)
    {
        // log
        std::cout << "ReceiveReceivablesQueryAck::get receivables error\n";
        return;
    }

    for (const auto& i : *receivables_o)
    {
        auto amount_o = i.second.get_optional<std::string>("amount");
        if (!amount_o)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck::get amount error\n";
            return;
        }
        rai::Amount amount;
        error = amount.DecodeDec(*amount_o);
        if (error)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck::decode amount error\n";
            return;
        }

        auto source_o = i.second.get_optional<std::string>("source");
        if (!source_o)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck::get source error\n";
            return;
        }
        rai::Account source;
        error = source.DecodeAccount(*source_o);
        if (error)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck::decode source error\n";
            return;
        }

        auto hash_o = i.second.get_optional<std::string>("hash");
        if (!hash_o)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck::get hash error\n";
            return;
        }
        rai::BlockHash hash;
        error = hash.DecodeHex(*hash_o);
        if (error)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck::decode hash error\n";
            return;
        }

        auto timestamp_o = i.second.get_optional<std::string>("timestamp");
        if (!timestamp_o)
        {
            // log
            std::cout << "ReceiveReceivableInfoNotify::get timestamp error\n";
            return;
        }
        uint64_t timestamp;
        error = rai::StringToUint(*timestamp_o, timestamp);
        if (error)
        {
            // log
            std::cout << "ReceiveReceivableInfoNotify: invalid timestamp\n";
            return;
        }

        QueueReceivable(account, hash, amount, source, timestamp);
    }
}

void rai::Wallets::ReceiveReceivableInfoNotify(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto account_o = message->get_optional<std::string>("account");
    if (!account_o)
    {
        //log
        return;
    }
    rai::Account account;
    bool error = account.DecodeAccount(*account_o);
    if (error)
    {
        // log
        return;
    }

    auto amount_o = message->get_optional<std::string>("amount");
    if (!amount_o)
    {
        // log
        return;
    }
    rai::Amount amount;
    error = amount.DecodeDec(*amount_o);
    if (error)
    {
        // log
        return;
    }

    auto source_o = message->get_optional<std::string>("source");
    if (!source_o)
    {
        // log
        return;
    }
    rai::Account source;
    error = source.DecodeAccount(*source_o);
    if (error)
    {
        // log
        return;
    }

    auto hash_o = message->get_optional<std::string>("hash");
    if (!hash_o)
    {
        // log
        return;
    }
    rai::BlockHash hash;
    error = hash.DecodeHex(*hash_o);
    if (error)
    {
        // log
        return;
    }

    auto timestamp_o = message->get_optional<std::string>("timestamp");
    if (!timestamp_o)
    {
        // log
        return;
    }
    uint64_t timestamp;
    error = rai::StringToUint(*timestamp_o, timestamp);
    if (error)
    {
        // log
        std::cout << "ReceiveReceivableInfoNotify: invalid timestamp\n";
        return;
    }

    QueueReceivable(account, hash, amount, source, timestamp);
}

void rai::Wallets::ReceiveMessage(const std::shared_ptr<rai::Ptree>& message)
{
    std::cout << "receive message<<=====:\n";
    std::stringstream ostream;
    boost::property_tree::write_json(ostream, *message);
    ostream.flush();
    std::string body = ostream.str();
    std::cout << body << std::endl;

    auto ack_o = message->get_optional<std::string>("ack");
    if (ack_o)
    {
        std::string ack(*ack_o);
        if (ack == "block_query")
        {
            ReceiveBlockQueryAck(message);
        }
        else if (ack == "receivables")
        {
            ReceiveReceivablesQueryAck(message);
        }
        else if (ack == "account_info")
        {
            ReceiveAccountInfoQueryAck(message);
        }
    }

    auto notify_o = message->get_optional<std::string>("notify");
    if (notify_o)
    {
        std::string notify(*notify_o);
        if (notify == "block_append")
        {
            ReceiveBlockAppendNotify(message);
        }
        else if (notify == "block_confirm")
        {
            ReceiveBlockConfirmNotify(message);
        }
        else if (notify == "block_rollback")
        {
            ReceiveBlockRollbackNotify(message);
        }
        else if (notify == "receivable_info")
        {
            ReceiveReceivableInfoNotify(message);
        }
    }
}

bool rai::Wallets::Received(const rai::BlockHash& hash) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return received_.find(hash) != received_.end();
}

void rai::Wallets::ReceivedAdd(const rai::BlockHash& hash)
{
    std::lock_guard<std::mutex> lock(mutex_);
    received_.insert(hash);
}

void rai::Wallets::ReceivedDel(const rai::BlockHash& hash)
{
    std::lock_guard<std::mutex> lock(mutex_);
    received_.erase(hash);
}

std::vector<std::shared_ptr<rai::Wallet>> rai::Wallets::SharedWallets() const
{
    std::vector<std::shared_ptr<rai::Wallet>> result;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& i : wallets_)
    {
        result.push_back(i.second);
    }
    return result;
}

void rai::Wallets::Start()
{
    RegisterObservers_();

    Ongoing(std::bind(&rai::Wallets::ConnectToServer, this),
            std::chrono::seconds(5));

    Ongoing([this]() { this->SubscribeAll(); }, std::chrono::seconds(300));
    Ongoing([this]() { this->SyncAll(); }, std::chrono::seconds(300));
}

void rai::Wallets::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }
        stopped_ = true;
    }

    UnsubscribeAll();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    condition_.notify_all();
    if (thread_.joinable())
    {
        thread_.join();
    }
    std::cout << "wallets run thread closed\n";

    alarm_.Stop();
    std::cout << "alarm thread closed\n";
    websocket_.Close();
    std::cout << "websocket closed\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    service_runner_.Stop();
    std::cout << "io service thread closed\n";

    std::cout << "all closed\n";
}

void rai::Wallets::SelectAccount(uint32_t account_id)
{
    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex_);

        wallet = Wallet_(selected_wallet_id_);
        if (wallet == nullptr)
        {
            // log
            return;
        }

        if (wallet->SelectAccount(account_id))
        {
            return;
        }

        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return;
        }
        error_code = wallet->StoreInfo(transaction, selected_wallet_id_);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            transaction.Abort();
            return;
        }
    }

    if (selected_account_observer_)
    {
        selected_account_observer_(wallet->SelectedAccount());
    }
}

void rai::Wallets::SelectWallet(uint32_t wallet_id)
{
    rai::Account selected_account;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (wallet_id == selected_wallet_id_)
        {
            return;
        }

        bool exists = false;
        for (const auto& i : wallets_)
        {
            if (wallet_id == i.first)
            {
                exists = true;
                selected_account = i.second->SelectedAccount();
                break;
            }
        }
        if (!exists)
        {
            return;
        }

        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return;
        }
        bool error = ledger_.SelectedWalletIdPut(transaction, wallet_id);
        if (error)
        {
            // log
            transaction.Abort();
            return;
        }
        selected_wallet_id_ = wallet_id;
    }

    if (selected_wallet_observer_)
    {
        selected_wallet_observer_(wallet_id);
    }

    if (selected_account_observer_)
    {
        selected_account_observer_(selected_account);
    }
}

rai::Account rai::Wallets::SelectedAccount() const
{
    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wallet = Wallet_(selected_wallet_id_);
    }

    if (wallet)
    {
        return wallet->SelectedAccount();
    }

    return rai::Account(0);
}

std::shared_ptr<rai::Wallet> rai::Wallets::SelectedWallet() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return Wallet_(selected_wallet_id_);
}

uint32_t rai::Wallets::SelectedWalletId() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return selected_wallet_id_;
}

void rai::Wallets::Subscribe(const std::shared_ptr<rai::Wallet>& wallet,
                             uint32_t account_id)
{
    Subscribe_(wallet, account_id);
}

void rai::Wallets::Subscribe(const std::shared_ptr<rai::Wallet>& wallet)
{
    if (wallet == nullptr)
    {
        return;
    }
    auto accounts = wallet->Accounts();

    for (const auto& i : accounts)
    {
        Subscribe_(wallet, i.first);
    }
}

void rai::Wallets::SubscribeAll()
{
    auto wallets = SharedWallets();
    for (const auto& wallet : wallets)
    {
        Subscribe(wallet);
    }
}

void rai::Wallets::Sync(const rai::Account& account)
{
    SyncBlocks(account);
    SyncForks(account); 
    SyncAccountInfo(account);
    SyncReceivables(account);
}

void rai::Wallets::Sync(const std::shared_ptr<rai::Wallet>& wallet)
{
    SyncBlocks(wallet);
    SyncForks(wallet); 
    SyncAccountInfo(wallet);
    SyncReceivables(wallet);
}

void rai::Wallets::SyncAll()
{
    auto wallets = SharedWallets();
    for (const auto& wallet : wallets)
    {
        Sync(wallet);
    }
}

void rai::Wallets::SyncAccountInfo(const rai::Account& account)
{
    AccountInfoQuery(account);
}

void rai::Wallets::SyncAccountInfo(const std::shared_ptr<rai::Wallet>& wallet)
{
    if (wallet == nullptr)
    {
        return;
    }
    auto accounts = wallet->Accounts();

    for (const auto& i : accounts)
    {
       AccountInfoQuery(i.second.first);
    }
}

void rai::Wallets::SyncBlocks(const rai::Account& account)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    SyncBlocks_(transaction, account);
}

void rai::Wallets::SyncBlocks(const std::shared_ptr<rai::Wallet>& wallet)
{
    if (wallet == nullptr)
    {
        return;
    }
    auto accounts = wallet->Accounts();

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    for (const auto& i : accounts)
    {
       SyncBlocks_(transaction, i.second.first);
    }
}

void rai::Wallets::SyncForks(const rai::Account& account)
{
    static_assert(RAI_TODO, "Wallets::SyncForks");
}

void rai::Wallets::SyncForks(const std::shared_ptr<rai::Wallet>& wallet)
{
    static_assert(RAI_TODO, "Wallets::SyncForks");
}

void rai::Wallets::SyncReceivables(const rai::Account& account)
{
    ReceivablesQuery(account);
}

void rai::Wallets::SyncReceivables(const std::shared_ptr<rai::Wallet>& wallet)
{
    if (wallet == nullptr)
    {
        return;
    }
    auto accounts = wallet->Accounts();

    for (const auto& i : accounts)
    {
       ReceivablesQuery(i.second.first);
    }
}

void rai::Wallets::Unsubscribe(const std::shared_ptr<rai::Wallet>& wallet)
{
    if (wallet == nullptr)
    {
        return;
    }
    auto accounts = wallet->Accounts();

    for (const auto& i : accounts)
    {
        rai::Account account = i.second.first;
        uint64_t timestamp = rai::CurrentTimestamp();

        rai::Ptree ptree;
        ptree.put("action", "account_unsubscribe");
        ptree.put("account", i.second.first.StringAccount());

        websocket_.Send(ptree);
    }
}

void rai::Wallets::UnsubscribeAll()
{
    auto wallets = SharedWallets();
    for (const auto& wallet : wallets)
    {
        Unsubscribe(wallet);
    }
}

std::vector<uint32_t> rai::Wallets::WalletIds() const
{
    std::vector<uint32_t> result;
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& i : wallets_)
    {
        result.push_back(i.first);
    }
    return result;
}

void rai::Wallets::WalletBalanceAll(uint32_t wallet_id, rai::Amount& balance,
                                    rai::Amount& confirmed,
                                    rai::Amount& receivable)
{
    balance.Clear();
    confirmed.Clear();
    receivable.Clear();

    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wallet = Wallet_(wallet_id);
        if (wallet == nullptr)
        {
            return;
        }
    }

    auto accounts = wallet->Accounts();
    for (const auto& i : accounts)
    {
        rai::Amount balance_l;
        rai::Amount confirmed_l;
        rai::Amount receivable_l;
        AccountBalanceAll(i.second.first, balance_l, confirmed_l, receivable_l);
        balance += balance_l;
        confirmed += confirmed_l;
        receivable += receivable_l;
    }
}

uint32_t rai::Wallets::WalletAccounts(uint32_t wallet_id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto wallet = Wallet_(wallet_id);
    if (wallet == nullptr)
    {
        return 0;
    }

    return static_cast<uint32_t>(wallet->Accounts().size());
}

rai::ErrorCode rai::Wallets::WalletSeed(uint32_t wallet_id,
                                        rai::RawKey& seed) const
{
    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wallet = Wallet_(wallet_id);
        if (wallet == nullptr)
        {
            return rai::ErrorCode::WALLET_GET;
        }
    }

    return wallet->Seed(seed);
}

bool rai::Wallets::WalletLocked(uint32_t wallet_id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto wallet = Wallet_(wallet_id);
    if (wallet != nullptr && wallet->ValidPassword())
    {
        return false;
    }

    return true;
}

bool rai::Wallets::WalletVulnerable(uint32_t wallet_id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto wallet = Wallet_(wallet_id);
    if (wallet != nullptr && wallet->EmptyPassword())
    {
        return true;
    }

    return false;
}

void rai::Wallets::InitReceived_(rai::Transaction& transaction)
{
    for (auto i = ledger_.AccountInfoBegin(transaction),
              n = ledger_.AccountInfoEnd(transaction);
         i != n; ++i)
    {
        rai::Account account;
        rai::AccountInfo info;
        bool error = ledger_.AccountInfoGet(i, account, info);
        if (error)
        {
            transaction.Abort();
            throw std::runtime_error("AccountInfoGet error");
        }

        if (info.head_height_ == rai::Block::INVALID_HEIGHT)
        {
            continue;
        }

        rai::BlockHash hash(info.head_);
        while (!hash.IsZero())
        {
            std::shared_ptr<rai::Block> block(nullptr);
            error = ledger_.BlockGet(transaction, hash, block);
            if (error)
            {
                transaction.Abort();
                throw std::runtime_error("BlockGet error");
            }

            if (block->Opcode() == rai::BlockOpcode::RECEIVE)
            {
                received_.insert(block->Link());
            }
            hash = block->Previous();
        }
    }
}

uint32_t rai::Wallets::NewWalletId_() const
{
    uint32_t result = 1;

    for (const auto& i : wallets_)
    {
        if (i.first >= result)
        {
            result = i.first + 1;
        }
    }

    return result;
}

void rai::Wallets::RegisterObservers_()
{
    websocket_.message_processor_ =
        [this](const std::shared_ptr<rai::Ptree>& message) {
            this->ReceiveMessage(message);
        };

    std::weak_ptr<rai::Wallets> wallets(Shared());
    websocket_.status_observer_ = [wallets](rai::WebsocketStatus status) {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets, status]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.connection_status_.Notify(status);
            }
        });
    };

    block_observer_ = [wallets](const std::shared_ptr<rai::Block>& block,
                                bool rollback) {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets, block, rollback]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.block_.Notify(block, rollback);
            }
        });
    };

    selected_account_observer_ = [wallets](const rai::Account& account) {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets, account]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.selected_account_.Notify(account);
            }
        });
    };

    selected_wallet_observer_ = [wallets](uint32_t wallet_id){
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets, wallet_id]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.selected_wallet_.Notify(wallet_id);
            }
        });
    };

    wallet_locked_observer_ = [wallets](bool locked) {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets, locked]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.wallet_locked_.Notify(locked);
            }
        });
    };

    wallet_password_set_observer_ = [wallets]() {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.wallet_password_set_.Notify();
            }
        });
    };

    receivable_ = [wallets](const rai::Account& account) {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets, account]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.receivable_.Notify(account);
            }
        });
    };

    observers_.connection_status_.Add([this](rai::WebsocketStatus status) {
        if (status == rai::WebsocketStatus::CONNECTED)
        {
            std::cout << "websocket connected\n";
            SubscribeAll();
            SyncAll();
        }
        else if (status == rai::WebsocketStatus::CONNECTING)
        {
            std::cout << "websocket connecting\n";
        }
        else
        {
            std::cout << "websocket disconnected\n";
        }
    });

    observers_.block_.Add(
        [this](const std::shared_ptr<rai::Block>& block, bool rollback) {
            SyncBlocks(block->Account());

            if (rollback && block->Opcode() == rai::BlockOpcode::RECEIVE)
            {
                SyncReceivables(block->Account());
            }
        });

}

bool rai::Wallets::Rollback_(rai::Transaction& transaction,
                             const rai::Account& account)
{
    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        std::cout << "Wallets::Rollback_::AccountInfoGet error!" << std::endl;
        return true;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, block);
    if (error)
    {
        //log
        std::cout << "Wallets::Rollback_::BlockGet error!" << std::endl;
        return true;
    }

    error = ledger_.RollbackBlockPut(transaction, info.head_, *block);
    if (error)
    {
        //log
        std::cout << "Wallets::Rollback_::RollbackBlockPut error!" << std::endl;
        return true;
    }

    error = ledger_.BlockDel(transaction, info.head_);
    if (error)
    {
        //log
        std::cout << "Wallets::Rollback_::BlockDel error!" << std::endl;
        return true;
    }
    if (block->Height() != 0)
    {
        error = ledger_.BlockSuccessorSet(transaction, block->Previous(),
                                          rai::BlockHash(0));
        IF_ERROR_RETURN(error, true);
    }

    if (info.head_height_ == 0)
    {
        error = ledger_.AccountInfoDel(transaction, account);
        if (error)
        {
            //log
            std::cout << "Wallets::Rollback_::AccountInfoDel error!" << std::endl;
            return true;
        }
    }
    else
    {
        info.head_ = block->Previous();
        info.head_height_ -= 1;
        if (info.confirmed_height_ != rai::Block::INVALID_HEIGHT
            && info.confirmed_height_ > info.head_height_)
        {
            info.confirmed_height_ = info.head_height_;
        }
        error = ledger_.AccountInfoPut(transaction, account, info);
        if (error)
        {
            //log
            std::cout << "Wallets::Rollback_::AccountInfoPut error!" << std::endl;
            return true;
        }
    }

    if (block->Opcode() == rai::BlockOpcode::RECEIVE)
    {
        ReceivedDel(block->Link());
    }

    return false;
}

void rai::Wallets::Subscribe_(const std::shared_ptr<rai::Wallet>& wallet,
                              uint32_t account_id)
{
    rai::Account account = wallet->Account(account_id);
    if (account.IsZero())
    {
        return;
    }

    uint64_t timestamp   = rai::CurrentTimestamp();

    rai::Ptree ptree;
    ptree.put("action", "account_subscribe");
    ptree.put("account", account.StringAccount());
    ptree.put("timestamp", std::to_string(rai::CurrentTimestamp()));

    if (wallet->ValidPassword())
    {
        int ret;
        rai::uint256_union hash;
        blake2b_state state;

        ret = blake2b_init(&state, hash.bytes.size());
        assert(0 == ret);

        std::vector<uint8_t> bytes;
        {
            rai::VectorStream stream(bytes);
            rai::Write(stream, account.bytes);
            rai::Write(stream, timestamp);
        }
        blake2b_update(&state, bytes.data(), bytes.size());

        ret = blake2b_final(&state, hash.bytes.data(), hash.bytes.size());
        assert(0 == ret);

        rai::Signature signature;
        bool error = wallet->Sign(account, hash, signature);
        if (error)
        {
            // log
            std::cout << "Failed to sign subscribe message (account="
                      << account.StringAccount() << ")\n";
        }
        else
        {
            ptree.put("signature", signature.StringHex());
        }
    }

    websocket_.Send(ptree);
}

void rai::Wallets::SyncBlocks_(rai::Transaction& transaction,
                               const rai::Account& account)
{
    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    if (error)
    {
        BlockQuery(account, 0, rai::BlockHash(0));
        return;
    }
    
    if (info.confirmed_height_ == rai::Block::INVALID_HEIGHT)
    {
        BlockQuery(account, 0, rai::BlockHash(0));
    }
    else
    {
        std::shared_ptr<rai::Block> block(nullptr);
        error = ledger_.BlockGet(transaction, account, info.confirmed_height_,
                                 block);
        if (error)
        {
            // log
            return;
        }
        BlockQuery(account, info.confirmed_height_ + 1, block->Hash());
    }

    if (info.head_height_ != info.confirmed_height_)
    {
        BlockQuery(account, info.head_height_ + 1, info.head_);
    }
}

std::shared_ptr<rai::Wallet> rai::Wallets::Wallet_(uint32_t wallet_id) const
{
    std::shared_ptr<rai::Wallet> result(nullptr);
    for (const auto& i : wallets_)
    {
        if (i.first == wallet_id)
        {
            result = i.second;
            break;
        }
    }
    return result;
}