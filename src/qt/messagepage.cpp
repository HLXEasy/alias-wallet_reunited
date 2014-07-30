#include "messagepage.h"
#include "ui_messagepage.h"

#include "sendmessagesdialog.h"
#include "messagemodel.h"
#include "bitcoingui.h"
#include "csvmodelwriter.h"
#include "guiutil.h"

#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>
#include <QStyledItemDelegate>
#include <QAbstractTextDocumentLayout>
#include <QPainter>

#define DECORATION_SIZE 64
#define NUM_ITEMS 3

class MessageViewDelegate : public QStyledItemDelegate
{
protected:
    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
};

void MessageViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItemV4 optionV4 = option;
    initStyleOption(&optionV4, index);

    QStyle *style = optionV4.widget? optionV4.widget->style() : QApplication::style();

    QTextDocument doc;
    QString align(index.data(MessageModel::TypeRole) == 1 ? "left" : "right");
    QString html = "<p align=\"" + align + "\" style=\"color:white;\">" + index.data(MessageModel::HTMLRole).toString() + "</p>";
    doc.setHtml(html);

    /// Painting item without text
    optionV4.text = QString();
    style->drawControl(QStyle::CE_ItemViewItem, &optionV4, painter);

    QAbstractTextDocumentLayout::PaintContext ctx;

    // Highlighting text if item is selected
    if (optionV4.state & QStyle::State_Selected)
        ctx.palette.setColor(QPalette::Text, optionV4.palette.color(QPalette::Active, QPalette::HighlightedText));

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &optionV4);
    doc.setTextWidth( textRect.width() );
    painter->save();
    painter->translate(textRect.topLeft());
    painter->setClipRect(textRect.translated(-textRect.topLeft()));
    doc.documentLayout()->draw(painter, ctx);
    painter->restore();
}

QSize MessageViewDelegate::sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    QStyleOptionViewItemV4 options = option;
    initStyleOption(&options, index);

    QTextDocument doc;
    doc.setHtml(index.data(MessageModel::HTMLRole).toString());
    doc.setTextWidth(options.rect.width());
    return QSize(doc.idealWidth(), doc.size().height() + 20);
}

#include "messagepage.moc"

MessagePage::MessagePage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MessagePage),
    model(0),
    msgdelegate(new MessageViewDelegate())
{
    ui->setupUi(this);

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->deleteButton->setIcon(QIcon());
#endif
    // Context menu actions
    replyAction           = new QAction(ui->sendButton->text(),           this);
    copyFromAddressAction = new QAction(ui->copyFromAddressButton->text(), this);
    copyToAddressAction   = new QAction(ui->copyToAddressButton->text(),   this);
    deleteAction          = new QAction(ui->deleteButton->text(),          this);

    // Build context menu
    contextMenu = new QMenu();

    contextMenu->addAction(replyAction);
    contextMenu->addAction(copyFromAddressAction);
    contextMenu->addAction(copyToAddressAction);
    contextMenu->addAction(deleteAction);

    connect(replyAction,           SIGNAL(triggered()), this, SLOT(on_sendButton_clicked()));
    connect(copyFromAddressAction, SIGNAL(triggered()), this, SLOT(on_copyFromAddressButton_clicked()));
    connect(copyToAddressAction,   SIGNAL(triggered()), this, SLOT(on_copyToAddressButton_clicked()));
    connect(deleteAction,          SIGNAL(triggered()), this, SLOT(on_deleteButton_clicked()));

    connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));

    // Show Messages
    ui->listConversation->setItemDelegate(msgdelegate);
    ui->listConversation->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listConversation->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listConversation->setAttribute(Qt::WA_MacShowFocusRect, false);

    //connect(ui->listConversation, SIGNAL(clicked(QModelIndex)), this, SLOT(handleMessageClicked(QModelIndex)));
}

MessagePage::~MessagePage()
{
    delete ui;
}

void MessagePage::setModel(MessageModel *model)
{
    this->model = model;
    if(!model)
        return;

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    proxyModel->setFilterRole(MessageModel::Ambiguous);
    proxyModel->setFilterFixedString("true");

    ui->tableView->setModel(proxyModel);
    ui->tableView->sortByColumn(MessageModel::ReceivedDateTime, Qt::DescendingOrder);

    ui->listConversation->setModel(proxyModel);
    ui->listConversation->setModelColumn(MessageModel::HTML);

    // Set column widths
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::Type,             100);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::Label,            100);
    ui->tableView->horizontalHeader()->setResizeMode(MessageModel::Label,            QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::FromAddress,      320);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::ToAddress,        320);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::SentDateTime,     170);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::ReceivedDateTime, 170);

    ui->messageEdit->setMaximumHeight(30);

    // Hidden columns
    ui->tableView->setColumnHidden(MessageModel::Message, true);

    connect(ui->tableView->selectionModel(),        SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(selectionChanged()));
    connect(ui->tableView,                          SIGNAL(doubleClicked(QModelIndex)),                       this, SLOT(selectionChanged()));
    connect(ui->listConversation->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),  this, SLOT(itemSelectionChanged()));
    connect(ui->listConversation,                   SIGNAL(doubleClicked(QModelIndex)),                       this, SLOT(itemSelectionChanged()));
    connect(ui->messageEdit,                        SIGNAL(textChanged()),                                    this, SLOT(messageTextChanged()));

    selectionChanged();
}

void MessagePage::on_sendButton_clicked()
{
    if(!model)
        return;

    std::string sError;
    std::string sendTo  = replyToAddress.toStdString();
    std::string message = ui->messageEdit->toHtml().toStdString();
    std::string addFrom = replyFromAddress.toStdString();

    if (SecureMsgSend(addFrom, sendTo, message, sError) != 0)
    {
        QMessageBox::warning(NULL, tr("Send Secure Message"),
            tr("Send failed: %1.").arg(sError.c_str()),
            QMessageBox::Ok, QMessageBox::Ok);

        return;
    };

    ui->messageEdit->setMaximumHeight(30);
    ui->messageEdit->clear();
    ui->listConversation->scrollToBottom();
}

void MessagePage::on_newButton_clicked()
{
    if(!model)
        return;

    SendMessagesDialog dlg(SendMessagesDialog::Encrypted, SendMessagesDialog::Dialog, this);

    dlg.setModel(model);
    dlg.exec();
}

void MessagePage::on_copyFromAddressButton_clicked()
{
    GUIUtil::copyEntryData(ui->tableView, MessageModel::FromAddress, Qt::DisplayRole);
}

void MessagePage::on_copyToAddressButton_clicked()
{
    GUIUtil::copyEntryData(ui->tableView, MessageModel::ToAddress, Qt::DisplayRole);
}

void MessagePage::on_deleteButton_clicked()
{
    QListView *list = ui->listConversation;

    if(!list->selectionModel())
        return;

    QModelIndexList indexes = list->selectionModel()->selectedIndexes();

    if(!indexes.isEmpty())
    {
        list->model()->removeRow(indexes.at(0).row());
    }
}

void MessagePage::on_backButton_clicked()
{
    model->resetFilter();

    proxyModel->setFilterFixedString("");
    proxyModel->setFilterRole(MessageModel::Ambiguous);
    proxyModel->setFilterFixedString("true");

    ui->listConversation->clearFocus();
    itemSelectionChanged();
    selectionChanged();

    ui->messageDetails->hide();
    ui->tableView->show();
    ui->newButton->setEnabled(true);
    ui->newButton->setVisible(true);
    ui->sendButton->setEnabled(false);
    ui->sendButton->setVisible(false);
    ui->messageEdit->setVisible(false);
}

void MessagePage::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;

    if(table->selectionModel()->hasSelection())
    {
        replyAction->setEnabled(true);
        copyFromAddressAction->setEnabled(true);
        copyToAddressAction->setEnabled(true);
        deleteAction->setEnabled(true);

        ui->copyFromAddressButton->setEnabled(true);
        ui->copyToAddressButton->setEnabled(true);
        ui->deleteButton->setEnabled(true);

        ui->newButton->setEnabled(false);
        ui->newButton->setVisible(false);
        ui->sendButton->setEnabled(true);
        ui->sendButton->setVisible(true);
        ui->messageEdit->setVisible(true);

        ui->tableView->hide();

        // Figure out which message was selected
        QModelIndexList labelColumn       = table->selectionModel()->selectedRows(MessageModel::Label);
        QModelIndexList addressFromColumn = table->selectionModel()->selectedRows(MessageModel::FromAddress);
        QModelIndexList addressToColumn   = table->selectionModel()->selectedRows(MessageModel::ToAddress);

        foreach (QModelIndex index, labelColumn)
            ui->contactLabel->setText(table->model()->data(index).toString());

        foreach (QModelIndex index, addressFromColumn)
            replyToAddress = table->model()->data(index).toString();

        foreach (QModelIndex index, addressToColumn)
            replyFromAddress = table->model()->data(index).toString();

        proxyModel->sort(MessageModel::ReceivedDateTime);
        QString filter = table->selectionModel()->model()->data(table->selectionModel()->selectedRows(MessageModel::FromAddress)[0], Qt::DisplayRole).toString();
        proxyModel->setFilterRole(MessageModel::FromAddressRole);
        proxyModel->setFilterFixedString(filter);
        ui->messageDetails->show();
    }
    else
    {
        ui->newButton->setEnabled(true);
        ui->newButton->setVisible(true);
        ui->sendButton->setEnabled(false);
        ui->sendButton->setVisible(false);
        ui->copyFromAddressButton->setEnabled(false);
        ui->copyToAddressButton->setEnabled(false);
        ui->deleteButton->setEnabled(false);
        ui->messageEdit->hide();
        ui->messageDetails->hide();
        ui->messageEdit->clear();
    }
}

void MessagePage::itemSelectionChanged()
{
    // Set button states based on selected tab and selection
    QListView *list = ui->listConversation;
    if(!list->selectionModel())
        return;

    if(list->selectionModel()->hasSelection())
    {
        replyAction->setEnabled(true);
        copyFromAddressAction->setEnabled(true);
        copyToAddressAction->setEnabled(true);
        deleteAction->setEnabled(true);

        ui->copyFromAddressButton->setEnabled(true);
        ui->copyToAddressButton->setEnabled(true);
        ui->deleteButton->setEnabled(true);

        ui->newButton->setEnabled(false);
        ui->newButton->setVisible(false);
        ui->sendButton->setEnabled(true);
        ui->sendButton->setVisible(true);
        ui->messageEdit->setVisible(true);

        ui->tableView->hide();

        // Figure out which message was selected
        QModelIndexList indexes = list->selectionModel()->selectedIndexes();

        /*
        foreach (QModelIndex index, indexes)
            replyToAddress = list->model()->data(index).toString();

        foreach (QModelIndex index, addressToColumn)
            replyFromAddress = list->model()->data(index).toString();
            */

    }
    else
    {
        ui->newButton->setEnabled(true);
        ui->newButton->setVisible(true);
        ui->sendButton->setEnabled(false);
        ui->sendButton->setVisible(false);
        ui->copyFromAddressButton->setEnabled(false);
        ui->copyToAddressButton->setEnabled(false);
        ui->deleteButton->setEnabled(false);
        ui->messageEdit->hide();
        ui->messageDetails->hide();
        ui->messageEdit->clear();
    }
}

void MessagePage::messageTextChanged()
{
    if(ui->messageEdit->toPlainText().endsWith("\n"))
    {
        ui->messageEdit->setMaximumHeight(80);
        ui->messageEdit->resize(256, ui->messageEdit->document()->size().height() + 10);
    }

}

void MessagePage::exportClicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(
            this,
            tr("Export Messages"), QString(),
            tr("Comma separated file (*.csv)"));

    if (filename.isNull()) return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.setModel(proxyModel);
    writer.addColumn("Type",             MessageModel::Type,             Qt::DisplayRole);
    writer.addColumn("Label",            MessageModel::Label,            Qt::DisplayRole);
    writer.addColumn("FromAddress",      MessageModel::FromAddress,      Qt::DisplayRole);
    writer.addColumn("ToAddress",        MessageModel::ToAddress,        Qt::DisplayRole);
    writer.addColumn("SentDateTime",     MessageModel::SentDateTime,     Qt::DisplayRole);
    writer.addColumn("ReceivedDateTime", MessageModel::ReceivedDateTime, Qt::DisplayRole);
    writer.addColumn("Message",          MessageModel::Message,          Qt::DisplayRole);

    if(!writer.write())
    {
        QMessageBox::critical(this, tr("Error exporting"), tr("Could not write to file %1.").arg(filename),
                              QMessageBox::Abort, QMessageBox::Abort);
    }
}


void MessagePage::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->tableView->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}
