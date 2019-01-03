// Copyright (c) 2011-2013 The Bitcoin Core developers
// Copyright (c) 2016-2019 The Spectrecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transactiontablemodel.h"
#include "guiutil.h"
#include "transactionrecord.h"
#include "guiconstants.h"
#include "transactiondesc.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"

#include "wallet.h"
#include "ui_interface.h"

#include <QLocale>
#include <QList>
#include <QColor>
#include <QIcon>
#include <QDateTime>
#include <QtAlgorithms>

// Amount column is right-aligned it contains numbers
static int column_alignments[] = {
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignRight|Qt::AlignVCenter
    };

// Comparison operator for sort/binary search of model tx list
struct TxLessThan
{
    bool operator()(const TransactionRecord &a, const TransactionRecord &b) const { return a.hash < b.hash; }
    bool operator()(const TransactionRecord &a, const uint256 &b)           const { return a.hash < b;      }
    bool operator()(const uint256 &a, const TransactionRecord &b)           const { return a      < b.hash; }
};

// Private implementation
class TransactionTablePriv
{
public:
    TransactionTablePriv(CWallet *wallet, TransactionTableModel *parent):
            wallet(wallet),
            parent(parent)
    {
    }
    CWallet *wallet;
    TransactionTableModel *parent;

    /* Local cache of wallet.
     * As it is in the same order as the CWallet, by definition
     * this is sorted by sha256.
     */
    QList<TransactionRecord> cachedWallet;

    /* Query entire wallet anew from core.
     */
    void refreshWallet()
    {
        cachedWallet.clear();

        {
            LOCK2(cs_main, wallet->cs_wallet);
            for (std::map<uint256, CWalletTx>::iterator it = wallet->mapWallet.begin(); it != wallet->mapWallet.end(); ++it)
                if (TransactionRecord::showTransaction(it->second))
                    cachedWallet.append(TransactionRecord::decomposeTransaction(wallet, it->second));
        }
    }

    /* Update our model of the wallet incrementally, to synchronize our model of the wallet
       with that of the core.

       Call with transaction that was added, removed or changed.
     */
    void updateWallet(const uint256 &hash, int status, bool showTransaction)
    {
        LogPrintf("updateWallet %s %i\n", hash.ToString().c_str(), status);
        {

            // Find bounds of this transaction in model
            QList<TransactionRecord>::iterator lower = qLowerBound(
                cachedWallet.begin(), cachedWallet.end(), hash, TxLessThan());
            QList<TransactionRecord>::iterator upper = qUpperBound(
                cachedWallet.begin(), cachedWallet.end(), hash, TxLessThan());
            int lowerIndex = (lower - cachedWallet.begin());
            int upperIndex = (upper - cachedWallet.begin());
            bool inModel = (lower != upper);

            if (status == CT_UPDATED)
            {
                if (showTransaction && !inModel)
                    status = CT_NEW; /* Not in model, but want to show, treat as new */
                if (!showTransaction && inModel)
                    status = CT_DELETED; /* In model, but want to hide, treat as deleted */
            };

            LogPrintf("   inModel=%i Index=%i-%i showTransaction=%i derivedStatus=%i\n",
                        inModel, lowerIndex, upperIndex, showTransaction, status);

            switch(status)
            {
            case CT_NEW:
                if(inModel)
                {
                    LogPrintf("Warning: updateWallet: Got CT_NEW, but transaction is already in model\n");
                    break;
                }
                if(showTransaction)
                {
                    QList<TransactionRecord> toInsert;

                    {
                        LOCK2(cs_main, wallet->cs_wallet);
                        // Find transaction in wallet
                        std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(hash);
                        if(mi == wallet->mapWallet.end())
                        {
                            LogPrintf("Warning: updateWallet: Got CT_NEW, but transaction is not in wallet\n");
                            break;
                        }
                        // Added -- insert at the right position
                        toInsert = TransactionRecord::decomposeTransaction(wallet, mi->second);
                    }

                    if(!toInsert.isEmpty()) /* only if something to insert */
                    {
                        parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex+toInsert.size()-1);
                        int insert_idx = lowerIndex;
                        Q_FOREACH(const TransactionRecord &rec, toInsert)
                        {
                            cachedWallet.insert(insert_idx, rec);
                            insert_idx += 1;
                        }
                        parent->endInsertRows();
                    }
                }
                break;
                break;
            case CT_DELETED:
                if(!inModel)
                {
                    LogPrintf("Warning: updateWallet: Got CT_DELETED, but transaction is not in model\n");
                    break;
                }
                // Removed -- remove entire transaction from table
                parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex-1);
                cachedWallet.erase(lower, upper);
                parent->endRemoveRows();
                break;
            case CT_UPDATED:
                // Miscellaneous updates -- nothing to do, status update will take care of this, and is only computed for
                // visible transactions.
                break;
            }
        }
    }

    int size()
    {
        return cachedWallet.size();
    }

    TransactionRecord *index(int idx)
    {
        if(idx >= 0 && idx < cachedWallet.size())
        {
            TransactionRecord *rec = &cachedWallet[idx];

            // Get required locks upfront. This avoids the GUI from getting
            // stuck if the core is holding the locks for a longer time - for
            // example, during a wallet rescan.
            //
            // If a status update is needed (blocks came in since last check),
            //  update the status of this transaction from the wallet. Otherwise,
            // simply re-use the cached status.
            TRY_LOCK(cs_main, lockMain);

            if(lockMain && rec->statusUpdateNeeded())
            {
                TRY_LOCK(wallet->cs_wallet, lockWallet);
                if(lockWallet)
                {
                    std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(rec->hash);

                    if(mi != wallet->mapWallet.end())
                        rec->updateStatus(mi->second);
                }
            }
            return rec;
        }
        else
            return 0;
    }

    QString describe(TransactionRecord *rec)
    {
        {
            LOCK2(cs_main, wallet->cs_wallet);

            std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(rec->hash);
            if(mi != wallet->mapWallet.end())
                return TransactionDesc::toHTML(wallet, mi->second);
        }

        return QString("");
    }

};

TransactionTableModel::TransactionTableModel(CWallet* wallet, WalletModel *parent):
        QAbstractTableModel(parent),
        wallet(wallet),
        walletModel(parent),
        priv(new TransactionTablePriv(wallet, this))
{
    columns << QString() << tr("Date") << tr("Type") << tr("Address") << tr("Narration") << tr("Amount");

    priv->refreshWallet();

    connect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    subscribeToCoreSignals();
}

TransactionTableModel::~TransactionTableModel()
{
    delete priv;
    unsubscribeFromCoreSignals();
}

void TransactionTableModel::updateTransaction(const QString &hash, int status, bool showTransaction)
{
    uint256 updated;
    updated.SetHex(hash.toStdString());

    priv->updateWallet(updated, status, showTransaction);
}

void TransactionTableModel::updateConfirmations()
{
    // Blocks came in since last poll.
    // Invalidate status (number of confirmations) and (possibly) description
    //  for all rows. Qt is smart enough to only actually request the data for the
    //  visible rows.
    emit dataChanged(index(0, Status), index(priv->size()-1, Status));
    emit dataChanged(index(0, ToAddress), index(priv->size()-1, ToAddress));
}

int TransactionTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int TransactionTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QString TransactionTableModel::formatTxStatus(const TransactionRecord *wtx) const
{
    QString status;

    switch(wtx->status.status)
    {
    case TransactionStatus::OpenUntilBlock:
        status = tr("Open for %n more block(s)","",wtx->status.open_for);
        break;
    case TransactionStatus::OpenUntilDate:
        status = tr("Open until %1").arg(GUIUtil::dateTimeStr(wtx->status.open_for));
        break;
    case TransactionStatus::Offline:
        status = tr("Offline");
        break;
    case TransactionStatus::Unconfirmed:
        status = tr("Unconfirmed");
        break;
    case TransactionStatus::Confirming:
        status = tr("Confirming (%1 of %2 recommended confirmations)").arg(wtx->status.depth).arg(TransactionRecord::RecommendedNumConfirmations);
        break;
    case TransactionStatus::Confirmed:
        status = tr("Confirmed (%1 confirmations)").arg(wtx->status.depth);
        break;
    case TransactionStatus::Conflicted:
        status = tr("Conflicted");
        break;
    case TransactionStatus::Immature:
        status = tr("Immature (%1 confirmations, will be available after %2)").arg(wtx->status.depth).arg(wtx->status.depth + wtx->status.matures_in);
        break;
    case TransactionStatus::MaturesWarning:
        status = tr("This block was not received by any other nodes and will probably not be accepted!");
        break;
    case TransactionStatus::NotAccepted:
        status = tr("Generated but not accepted");
        break;
    }

    return status;
}

QString TransactionTableModel::formatTxDate(const TransactionRecord *wtx) const
{
    return wtx->time ? GUIUtil::dateTimeStr(wtx->time) : "";
}

/* Look up address in address book, if found return label (address)
   otherwise just return (address)
 */
QString TransactionTableModel::lookupAddress(const std::string &address, bool tooltip) const
{
    if (address.empty())
        return "unknown";

    QString label = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(address));

    bool hasLabel = !label.isEmpty();
    QString description;

    if(hasLabel)
        description += label + QString(" ");

    if(!hasLabel || walletModel->getOptionsModel()->getDisplayAddresses() || tooltip)
    {
        if (hasLabel)
            description += QString("(");

        if (address.length() == 102)
            description += QString::fromStdString(address.substr(0, 34)) + "...";
        else
            description += QString::fromStdString(address);

        if (hasLabel)
            description += QString(")");
    }
    return description;
}

QVariant TransactionTableModel::txAddressDecoration(const TransactionRecord *wtx) const
{
    return QIcon(":/icons/tx_" + TransactionRecord::getTypeShort(wtx->type));
}

QString TransactionTableModel::formatTxToAddress(const TransactionRecord *wtx, bool tooltip) const
{
    switch(wtx->type)
    {
    case TransactionRecord::RecvFromOther:
        return QString::fromStdString(wtx->address);
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::SendToAddress:
    case TransactionRecord::Generated:
    case TransactionRecord::GeneratedSPECTRE:
    case TransactionRecord::GeneratedDonation:
	case TransactionRecord::GeneratedContribution:
    case TransactionRecord::RecvSpectre:
    case TransactionRecord::SendSpectre:
    case TransactionRecord::ConvertSPECTREtoXSPEC:
    case TransactionRecord::ConvertXSPECtoSPECTRE:
    case TransactionRecord::SendToSelfSPECTRE:
        return lookupAddress(wtx->address, tooltip);
    case TransactionRecord::SendToOther:
        return QString::fromStdString(wtx->address);
    case TransactionRecord::SendToSelf:
    default:
        return tr("(n/a)");
    }
}

QString TransactionTableModel::formatNarration(const TransactionRecord *wtx) const
{
    return QString::fromStdString(wtx->narration);
}


QVariant TransactionTableModel::addressColor(const TransactionRecord *wtx) const
{
    // Show addresses without label in a less visible color
    switch(wtx->type)
    {
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::SendToAddress:
    case TransactionRecord::Generated:
    case TransactionRecord::GeneratedSPECTRE:
    case TransactionRecord::GeneratedDonation:
    case TransactionRecord::GeneratedContribution:
    case TransactionRecord::RecvSpectre:
    case TransactionRecord::SendSpectre:
    case TransactionRecord::ConvertSPECTREtoXSPEC:
    case TransactionRecord::ConvertXSPECtoSPECTRE:
        {
        QString label = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(wtx->address));
        if(label.isEmpty())
            return COLOR_BAREADDRESS;
        } break;
    case TransactionRecord::SendToSelf:
    case TransactionRecord::SendToSelfSPECTRE:
        return COLOR_BAREADDRESS;
    default:
        break;
    }
    return QVariant();
}

QString TransactionTableModel::formatTxAmount(const TransactionRecord *wtx, bool showUnconfirmed) const
{
    QString str = BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), wtx->credit + wtx->debit);
    if(showUnconfirmed)
    {
        if(!wtx->status.countsForBalance)
        {
            str = QString("[") + str + QString("]");
        }
    }
    return QString(str);
}

QString TransactionTableModel::txStatusDecoration(const TransactionRecord *wtx) const
{
    qint64 status_switch;
    qint64 confirmations = wtx->status.depth;

    switch(wtx->status.status)
    {
    case TransactionStatus::OpenUntilBlock:
    case TransactionStatus::OpenUntilDate:
        return "blue";
    case TransactionStatus::Offline:
        return "grey";
    case TransactionStatus::Immature:
    case TransactionStatus::Confirming:
        status_switch = wtx->status.status == TransactionStatus::Confirming ? confirmations : (confirmations * 5 / nCoinbaseMaturity + 1);

        switch(status_switch)
        {
            case 1: return "fa-clock-o red";
            case 2: return "fa-clock-o lightred";
            case 3: return "fa-clock-o orange";
            case 4: return "fa-clock-o yellow";
            default: return "fa-clock-o green";
        };

    case TransactionStatus::Confirmed:
        return "fa-check-circle green";
    case TransactionStatus::Conflicted:
        return "fa-exclamation-triange orange";

    default:
        return "fa-question-circle black";
    }
}

QString TransactionTableModel::formatTooltip(const TransactionRecord *rec) const
{
    QString tooltip = formatTxStatus(rec) + QString("\n") + rec->getTypeLabel(rec->type);

    if(rec->type==TransactionRecord::RecvFromOther || rec->type==TransactionRecord::SendToOther ||
       rec->type==TransactionRecord::SendToAddress || rec->type==TransactionRecord::RecvWithAddress ||
       rec->type==TransactionRecord::SendSpectre || rec->type==TransactionRecord::RecvSpectre ||
       rec->type==TransactionRecord::SendToSelfSPECTRE ||
       rec->type==TransactionRecord::ConvertSPECTREtoXSPEC || rec->type==TransactionRecord::ConvertXSPECtoSPECTRE)
    {
        tooltip += QString(" ") + formatTxToAddress(rec, true);
    }
    return tooltip;
}

QVariant TransactionTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();
    TransactionRecord *rec = static_cast<TransactionRecord*>(index.internalPointer());

    switch(role)
    {
    case Qt::DecorationRole:
        switch(index.column())
        {
        case Status:
            return txStatusDecoration(rec);
        case ToAddress:
            return txAddressDecoration(rec);
        }
        break;
    case Qt::DisplayRole:
        switch(index.column())
        {
        case Date:
            return formatTxDate(rec);
        case Type:
            return rec->getTypeLabel();
        case ToAddress:
            return formatTxToAddress(rec, false);
        case Narration:
            return formatNarration(rec);
        case Amount:
            return formatTxAmount(rec);
        }
        break;
    case Qt::EditRole:
        // Edit role is used for sorting, so return the unformatted values
        switch(index.column())
        {
        case Status:
            return QString::fromStdString(rec->status.sortKey);
        case Date:
            return rec->time;
        case Type:
            return rec->getTypeLabel();
        case ToAddress:
            return formatTxToAddress(rec, true);
        case Amount:
            return rec->credit + rec->debit;
        }
        break;
    case Qt::ToolTipRole:
        return formatTooltip(rec);
    case Qt::TextAlignmentRole:
        return column_alignments[index.column()];
    case Qt::ForegroundRole:
        // Non-confirmed (but not immature) as transactions are grey
        if(!rec->status.countsForBalance && rec->status.status != TransactionStatus::Immature)
        {
            return COLOR_UNCONFIRMED;
        }
        if(index.column() == Amount && (rec->credit+rec->debit) < 0)
        {
            return COLOR_NEGATIVE;
        }
        if(index.column() == ToAddress)
        {
            return addressColor(rec);
        }
        break;
    case TypeRole:
        return rec->type;
    case DateRole:
        return QDateTime::fromTime_t(static_cast<uint>(rec->time));
    case LongDescriptionRole:
        return priv->describe(rec);
    case AddressRole:
        return QString::fromStdString(rec->address);
    case LabelRole:
        return walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(rec->address));
    case AmountRole:
        return rec->credit + rec->debit;
    case CurrencyRole:
        return rec->currency == SPECTRE ? "SPECTRE" : "XSPEC";
    case TxIDRole:
        return QString::fromStdString(rec->getTxID());
    case ConfirmedRole:
        return rec->status.countsForBalance;
    case FormattedAmountRole:
        return formatTxAmount(rec, false);
    case StatusRole:
        return rec->status.status;
    case ConfirmationsRole:
        return QVariant::fromValue<qlonglong>(rec->status.depth);
    }
    return QVariant();
}

QVariant TransactionTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        {
            return columns[section];
        }
        else if (role == Qt::TextAlignmentRole)
        {
            return column_alignments[section];
        } else if (role == Qt::ToolTipRole)
        {
            switch(section)
            {
            case Status:
                return tr("Transaction status. Hover over this field to show number of confirmations.");
            case Date:
                return tr("Date and time that the transaction was received.");
            case Type:
                return tr("Type of transaction.");
            case ToAddress:
                return tr("Destination address of transaction.");
            case Amount:
                return tr("Amount removed from or added to balance.");
            }
        }
    }
    return QVariant();
}

QModelIndex TransactionTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    TransactionRecord *data = priv->index(row);
    if(data)
    {
        return createIndex(row, column, priv->index(row));
    }
    else
    {
        return QModelIndex();
    }
}

int TransactionTableModel::lookupTransaction(const QString &txid) const
{
    QModelIndexList lst = match(index(0, 0, QModelIndex()),
                                TxIDRole, txid, 1, Qt::MatchExactly);
    if(lst.isEmpty())
    {
        return -1;
    }
    else
    {
        return lst.at(0).row();
    }
}

void TransactionTableModel::updateDisplayUnit()
{
    // emit dataChanged to update Amount column with the current unit
    emit dataChanged(index(0, Amount), index(priv->size()-1, Amount));
}

// queue notifications to show a non freezing progress dialog e.g. for rescan
struct TransactionNotification
{
public:
    TransactionNotification() {}
    TransactionNotification(uint256 hash, ChangeType status, bool showTransaction):
        hash(hash), status(status), showTransaction(showTransaction) {}

    void invoke(QObject *ttm)
    {
        LogPrintf("NotifyTransactionChanged: %s status= %i", hash.GetHex(), status);
        QMetaObject::invokeMethod(ttm, "updateTransaction", Qt::QueuedConnection,
                                  Q_ARG(QString, QString::fromStdString(hash.GetHex())),
                                  Q_ARG(int, status),
                                  Q_ARG(bool, showTransaction));
    }
private:
    uint256 hash;
    ChangeType status;
    bool showTransaction;
};

static bool fQueueNotifications = false;
static std::vector< TransactionNotification > vQueueNotifications;

static void NotifyTransactionChanged(TransactionTableModel *ttm, CWallet *wallet, const uint256 &hash, ChangeType status)
{
    // Find transaction in wallet
    std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(hash);
    // Determine whether to show transaction or not (determine this here so that no relocking is needed in GUI thread)
    bool inWallet = mi != wallet->mapWallet.end();
    bool showTransaction = (inWallet && TransactionRecord::showTransaction(mi->second));

    TransactionNotification notification(hash, status, showTransaction);

    if (fQueueNotifications)
    {
        vQueueNotifications.push_back(notification);
        return;
    }
    notification.invoke(ttm);
}

void TransactionTableModel::subscribeToCoreSignals()
{
    // Connect signals to wallet
    wallet->NotifyTransactionChanged.connect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
    //wallet->ShowProgress.connect(boost::bind(ShowProgress, this, _1, _2)); TODO: Queue notifications...
}

void TransactionTableModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    wallet->NotifyTransactionChanged.disconnect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
    //wallet->ShowProgress.disconnect(boost::bind(ShowProgress, this, _1, _2));
}
