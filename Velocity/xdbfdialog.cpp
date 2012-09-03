#include "xdbfdialog.h"
#include "ui_xdbfdialog.h"

XdbfDialog::XdbfDialog(GPDBase *gpd, bool *modified, QWidget *parent) : QDialog(parent), ui(new Ui::XdbfDialog), gpd(gpd), modified(modified)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    ui->treeWidget->header()->resizeSection(0, 260);

    loadEntries();

    // display the gpd name
    for (DWORD i = 0; i < gpd->strings.size(); i++)
        if (gpd->strings.at(i).entry.id == TitleInformation)
        {
            if (gpd->strings.at(i).ws != L"")
                setWindowTitle("XDBF Viewer - " + QString::fromStdWString(gpd->strings.at(i).ws));
            break;
        }

    ui->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->treeWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
}

void XdbfDialog::addEntriesToTable(vector<XDBFEntry> entries, QString type)
{
    for (DWORD i = 0; i < entries.size(); i++)
    {
        // create an item
        QTreeWidgetItem *item = new QTreeWidgetItem;
        item->setText(0, QString::fromStdString(XDBFHelpers::IDtoString(entries.at(i).id)));
        item->setText(1, "0x" + QString::number(gpd->xdbf->GetRealAddress(entries.at(i).addressSpecifier), 16).toUpper());
        item->setText(2, "0x" + QString::number(entries.at(i).length, 16).toUpper());
        item->setText(3, type);

        // add it to the table
        ui->treeWidget->insertTopLevelItem(ui->treeWidget->topLevelItemCount(), item);
    }
}

void XdbfDialog::loadEntries()
{
    // dispay all the entries
    addEntriesToTable(gpd->xdbf->achievements.entries, "Achievement");
    addEntriesToTable(gpd->xdbf->images, "Image");
    addEntriesToTable(gpd->xdbf->settings.entries, "Setting");
    addEntriesToTable(gpd->xdbf->titlesPlayed.entries, "Title");
    addEntriesToTable(gpd->xdbf->strings, "String");
    addEntriesToTable(gpd->xdbf->avatarAwards.entries, "Avatar Award");
}

void XdbfDialog::showContextMenu(QPoint p)
{
    if (ui->treeWidget->selectedItems().count() < 1)
        return;

    QPoint globalPos = ui->treeWidget->mapToGlobal(p);
    QMenu contextMenu;

    contextMenu.addAction(QPixmap(":/Images/extract.png"), "Extract Entry");
    contextMenu.addAction(QPixmap(":/Images/replace.png"), "Replace Entry");
    contextMenu.addSeparator();
    contextMenu.addAction(QPixmap(":/Images/convert.png"), "Address Converter");
    contextMenu.addSeparator();
    contextMenu.addAction(QPixmap(":/Images/clean.png"), "Clean");

    QAction *selectedItem = contextMenu.exec(globalPos);

    if (selectedItem == NULL)
        return;
    else if (selectedItem->text() == "Extract Entry")
    {
        Entry e = indexToEntry(ui->treeWidget->currentIndex().row());
        XDBFEntry xentry;

        // get the xdbf entry
        if (e.type == Achievement)
            xentry = gpd->xdbf->achievements.entries.at(e.index);
        else if (e.type == Image)
            xentry = gpd->xdbf->images.at(e.index);
        else if (e.type == Setting)
            xentry = gpd->xdbf->settings.entries.at(e.index);
        else if (e.type == Title)
            xentry = gpd->xdbf->titlesPlayed.entries.at(e.index);
        else if (e.type == String)
            xentry = gpd->xdbf->strings.at(e.index);
        else if (e.type == AvatarAward)
            xentry = gpd->xdbf->avatarAwards.entries.at(e.index);

        // extract the entry into memory
        BYTE *entryBuff = new BYTE[xentry.length];
        gpd->xdbf->ExtractEntry(xentry, entryBuff);

        // get the out path
        QString outPath = QFileDialog::getSaveFileName(this, "Choose a place to extract the entry", QtHelpers::DesktopLocation() + "\\" + ui->treeWidget->currentItem()->text(0));

        if (outPath == "")
            return;

        // delete the file if it exists
        if (QFile::exists(outPath))
            QFile::remove(outPath);

        // write the data to a file
        FileIO io(outPath.toStdString(), true);
        io.write(entryBuff, xentry.length);
        io.close();

        // free the temporary memory
        delete[] entryBuff;
    }
    else if (selectedItem->text() == "Replace Entry")
    {
        QString entryPath = QFileDialog::getOpenFileName(this, "Open the entry to replace the current with", QtHelpers::DesktopLocation() + "\\" + ui->treeWidget->currentItem()->text(0));

        if (entryPath == "")
            return;

        Entry e = indexToEntry(ui->treeWidget->currentIndex().row());
        XDBFEntry xentry;

        // get the xdbf entry
        if (e.type == Achievement)
            xentry = gpd->xdbf->achievements.entries.at(e.index);
        else if (e.type == Image)
            xentry = gpd->xdbf->images.at(e.index);
        else if (e.type == Setting)
            xentry = gpd->xdbf->settings.entries.at(e.index);
        else if (e.type == Title)
            xentry = gpd->xdbf->titlesPlayed.entries.at(e.index);
        else if (e.type == String)
            xentry = gpd->xdbf->strings.at(e.index);
        else if (e.type == AvatarAward)
            xentry = gpd->xdbf->avatarAwards.entries.at(e.index);

        // open the file and get the length
        FileIO io(entryPath.toStdString());
        io.setPosition(0, ios_base::end);
        DWORD fileLen = io.getPosition();

        // allocate enough memory for the buffer
        BYTE *entryBuff = new BYTE[fileLen];

        // read in the file
        io.setPosition(0);
        io.readBytes(entryBuff, fileLen);

        gpd->xdbf->RewriteEntry(xentry, entryBuff);

        // cleanup
        delete[] entryBuff;
        io.close();
        if (modified != NULL)
            *modified = true;

        QMessageBox::information(this, "Success", "Successfully replaced the entry.");
    }
    else if (selectedItem->text() == "Address Converter")
    {
        AddressConverterDialog dialog(gpd->xdbf, this);
        dialog.exec();
    }
    else if (selectedItem->text() == "Clean")
    {
        QMessageBox::StandardButton btn = QMessageBox::question(this, "Continue?", "Cleaning the GPD will remove all of the unused memory that is in the file. This could potentially reduce the size of the GPD.\n\nDo you want to continue?", QMessageBox::Yes, QMessageBox::No);

        if (btn != QMessageBox::Yes)
            return;

        // clean the GPD
        try
        {
            gpd->xdbf->Clean();
        }
        catch (std::string error)
        {
            QMessageBox::critical(this, "Clean Error", "An error occured while cleaning the GPD.\n\n" + QString::fromStdString(error));
            return;
        }

        // reload the listing
        ui->treeWidget->clear();
        loadEntries();

        if (modified != NULL)
            *modified = true;

        QMessageBox::information(this, "Success", "Successfully cleaned the GPD.");
    }
}

XdbfDialog::~XdbfDialog()
{
    delete ui;
}

Entry XdbfDialog::indexToEntry(int index)
{
    Entry toReturn;

    DWORD count = 0;
    if (index < (count += gpd->xdbf->achievements.entries.size()))
    {
        toReturn.index = index;
        toReturn.type = Achievement;
    }
    else if (index < (count += gpd->xdbf->images.size()))
    {
        toReturn.index = index - gpd->xdbf->achievements.entries.size();
        toReturn.type = Image;
    }
    else if (index < (count += gpd->xdbf->settings.entries.size()))
    {
        toReturn.index = index - (count - gpd->xdbf->settings.entries.size());
        toReturn.type = Setting;
    }
    else if (index < (count += gpd->xdbf->titlesPlayed.entries.size()))
    {
        toReturn.index = index - (count - gpd->xdbf->titlesPlayed.entries.size());
        toReturn.type = Title;
    }
    else if (index < (count += gpd->xdbf->strings.size()))
    {
        toReturn.index = index - (count - gpd->xdbf->strings.size());
        toReturn.type = String;
    }
    else if (index < (count += gpd->xdbf->avatarAwards.entries.size()))
    {
        toReturn.index = index - (count - gpd->xdbf->avatarAwards.entries.size());
        toReturn.type = AvatarAward;
    }

    return toReturn;
}

void XdbfDialog::on_treeWidget_doubleClicked(const QModelIndex &index)
{
    Entry e = indexToEntry(index.row());
    switch (e.type)
    {
        case String:
            QMessageBox::about(this, "Setting", "<html><center><h3>Unicode String</h3><br />" + QString::fromStdWString(gpd->strings.at(e.index).ws) + "</center></html>");
            break;
        case Setting:
        {
            SettingEntry setting = gpd->settings.at(e.index);

            switch (setting.type)
            {
                case Int32:
                    QMessageBox::about(this, "Setting", "<html><center><h3>Int32 Setting</h3><br />" + QString::number(setting.int32) + "</center></html>");
                    break;
                case Int64:
                    QMessageBox::about(this, "Setting", "<html><center><h3>Int64 Setting</h3><br />" + QString::number(setting.int64) + "</center></html>");
                    break;
                case Float:
                    QMessageBox::about(this, "Setting", "<html><center><h3>Float Setting</h3><br />" + QString::number(setting.floatData) + "</center></html>");
                    break;
                case Double:
                    QMessageBox::about(this, "Setting", "<html><center><h3>Double Setting</h3><br />" + QString::number(setting.doubleData) + "</center></html>");
                    break;
                case UnicodeString:
                    QMessageBox::about(this, "Setting", "<html><center><h3>String Setting</h3><br />" + QString::fromStdWString(*setting.str) + "</center></html>");
                    break;
                case TimeStamp:
                    QMessageBox::about(this, "Setting", "<html><center><h3>Timestamp Setting</h3><br />" + QDateTime::fromTime_t(setting.timeStamp).toString() + "</center></html>");
                    break;
            }
            break;
        }
        case Image:
        {
            QByteArray imageBuff((char*)gpd->images.at(e.index).image, (size_t)gpd->images.at(e.index).length);

            ImageDialog dialog(QImage::fromData(imageBuff), this);
            dialog.exec();
            break;
        }
    }
}
