#include "deviceviewer.h"
#include "ui_deviceviewer.h"

#include <QDebug>

DeviceViewer::DeviceViewer(QStatusBar *statusBar, QWidget *parent) :
    QDialog(parent), ui(new Ui::DeviceViewer), parentEntry(NULL), statusBar(statusBar), currentDrive(NULL), drivesLoaded(false)
{
    ui->setupUi(this);

    ui->treeWidget->header()->setDefaultSectionSize(100);
    ui->treeWidget->header()->resizeSection(0, 250);

    // setup the context menus
    ui->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->treeWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));

    // setup treewdiget for drag and drop
    setAcceptDrops(true);
    ui->treeWidget->setAcceptDrops(true);
    connect(ui->treeWidget, SIGNAL(dragDropped(QDropEvent*)), this, SLOT(onDragDropped(QDropEvent*)));
    connect(ui->treeWidget, SIGNAL(dragEntered(QDragEnterEvent*)), this, SLOT(onDragEntered(QDragEnterEvent*)));
    connect(ui->treeWidget, SIGNAL(dragLeft(QDragLeaveEvent*)), this, SLOT(onDragLeft(QDragLeaveEvent*)));

    progressBar = new QProgressBar(this);
    progressBar->setMaximumHeight(statusBar->height() - 5);
    progressBar->setMinimumWidth(statusBar->width());
    progressBar->setTextVisible(false);
    progressBar->setVisible(false);
    statusBar->addWidget(progressBar);
}

DeviceViewer::~DeviceViewer()
{
    for (int i = 0; i < loadedDrives.size(); i++)
        delete loadedDrives.at(i);

    delete ui;
}

void DeviceViewer::DrawMemoryGraph()
{
    UINT64 totalFreeSpace = 0;
    UINT64 totalSpace = 0;

    if (!drivesLoaded)
    {
        progressBar->setVisible(true);
        progressBar->setMinimum(0);
        progressBar->setMaximum(0);
    }

    // load the partion information
    std::vector<Partition*> parts = currentDrive->GetPartitions();
    for (DWORD i = 0; i < parts.size(); i++)
    {
        totalFreeSpace += currentDrive->GetFreeMemory(parts.at(i), updateUI);
        totalSpace += (UINT64)parts.at(i)->clusterCount * parts.at(i)->clusterSize;
    }

    if (!drivesLoaded)
    {
        progressBar->setMaximum(1);
        progressBar->setVisible(false);
    }

    // calculate the percentage
    float freeMemPercentage = (((float)totalFreeSpace * 100.0) / totalSpace);

    // draw the insano piechart
    QPixmap chart(750, 500);
    chart.fill(ui->imgPiechart->palette().background().color());
    QPainter painter(&chart);
    Nightcharts pieChart;
    pieChart.setType(Nightcharts::Dpie);
    pieChart.setCords(25, 1, 700, 425);
    pieChart.setFont(QFont());
    pieChart.addPiece("Used Space", QColor(0, 0, 254), 100.0 - freeMemPercentage);
    pieChart.addPiece("Free Space", QColor(255, 0, 254), freeMemPercentage);
    pieChart.draw(&painter);

    ui->imgPiechart->setPixmap(chart);

    // setup the legend
    QPixmap freeMemClr(16, 16);
    freeMemClr.fill(QColor(255, 0, 254));
    ui->imgFreeMem->setPixmap(freeMemClr);
    ui->lblFeeMemory->setText(QString::fromStdString(ByteSizeToString(totalFreeSpace)) + " of Free Space");

    QPixmap usedMemClr(16, 16);
    usedMemClr.fill(QColor(0, 0, 254));
    ui->imgUsedMem->setPixmap(usedMemClr);
    ui->lblUsedSpace->setText(QString::fromStdString(ByteSizeToString(totalSpace - totalFreeSpace)) + " of Used Space");
}

void DeviceViewer::showContextMenu(QPoint point)
{
    QPoint globalPos = ui->treeWidget->mapToGlobal(point);
    QMenu contextMenu;

    QList<QTreeWidgetItem*> items = ui->treeWidget->selectedItems();

    foreach (QTreeWidgetItem *item, items)
        if (item->data(5, Qt::UserRole).toBool())
            return;

    if (parentEntry == NULL || parentEntry->name == "Drive Root")
        return;

    if (items.size() >= 1)
    {
        contextMenu.addAction(QPixmap(":/Images/extract.png"), "Copy Selected to Local Disk");
        contextMenu.addAction(QPixmap(":/Images/add.png"), "Copy File(s) Here");
        contextMenu.addAction(QPixmap(":/Images/FolderFileIcon.png"), "Create Folder Here");

        contextMenu.addSeparator();
        contextMenu.addAction(QPixmap(":/Images/delete.png"), "Delete Selected");
        contextMenu.addSeparator();

        contextMenu.addAction(QPixmap(":/Images/properties.png"), "View Properties");
    }
    else
    {
        contextMenu.addAction(QPixmap(":/Images/add.png"), "Copy File(s) Here");
        contextMenu.addAction(QPixmap(":/Images/FolderFileIcon.png"), "Create Folder Here");
    }

    QAction *selectedItem = contextMenu.exec(globalPos);
    if(selectedItem == NULL)
        return;

    try
    {
        if (selectedItem->text() == "Copy Selected to Local Disk")
        {
            QList<void*> filesToExtract;

            // get the entries
            for (int i = 0; i < items.size(); i++)
            {
                FatxFileEntry *entry = items.at(i)->data(0, Qt::UserRole).value<FatxFileEntry*>();
                if (entry->fileAttributes & FatxDirectory)
                    GetSubFiles(entry, filesToExtract);
                else
                    filesToExtract.push_back(entry);
            }

            // get the save path
            QString path = QFileDialog::getExistingDirectory(this, "Save Location", QDesktopServices::storageLocation(QDesktopServices::DesktopLocation));

            if (path.isEmpty())
                return;

            // save the file to the local disk
            MultiProgressDialog *dialog = new MultiProgressDialog(OpExtract, FileSystemFATX, currentDrive, path + "/", filesToExtract, this, QString::fromStdString(directoryChain.last()->path + directoryChain.last()->name + "\\"));
            dialog->setModal(true);
            dialog->show();
            dialog->start();
        }
        else if (selectedItem->text() == "View Properties")
        {
            FatxFileEntry *entry = items.at(0)->data(0, Qt::UserRole).value<FatxFileEntry*>();
            FatxFileDialog dialog(entry, entry->partition->clusterSize, items.at(0)->data(1, Qt::UserRole).toString(), this);
            dialog.exec();
        }
        else if (selectedItem->text() == "Copy File(s) Here")
        {
            QStringList toInjectPaths = QFileDialog::getOpenFileNames(this);
            if (toInjectPaths.size() == 0)
                return;

            QList<void*> files;
            for (DWORD i = 0; i < toInjectPaths.size(); i++)
                files.push_back(const_cast<void*>((void*)&toInjectPaths.at(i)));

            InjectFiles(files);
        }
        else if (selectedItem->text() == "Delete Selected")
        {
            // delete the entries
            for (int i = 0; i < items.size(); i++)
            {
                FatxFileEntry *entry = items.at(i)->data(0, Qt::UserRole).value<FatxFileEntry*>();
                currentDrive->DeleteFile(entry);

                QApplication::processEvents();

                DrawMemoryGraph();

                delete items.at(i);
            }
        }
        else if (selectedItem->text() == "Create Folder Here")
        {
            bool ok;
            QString text = QInputDialog::getText(this, "New Folder", "Folder name:", QLineEdit::Normal, "New Folder", &ok, windowFlags() & ~Qt::WindowContextHelpButtonHint);
            if (ok && !text.isEmpty())
            {
                if (text.length() > FATX_ENTRY_MAX_NAME_LENGTH)
                    QMessageBox::critical(this, "Invalid Filename", "The inputted name is invalid.");
                else
                    currentDrive->CreateFolder(parentEntry, text.toStdString());
            }
        }
    }
    catch (std::string error)
    {
        QMessageBox::warning(this, "Problem", "The operation failed.\n\n" + QString::fromStdString(error));
    }
}

void DeviceViewer::InjectFiles(QList<void*> files)
{
    MultiProgressDialog *dialog = new MultiProgressDialog(OpInject, FileSystemFATX, currentDrive, "", files, this, "", parentEntry);
    dialog->setModal(true);
    dialog->show();
    dialog->start();

    DrawMemoryGraph();

    LoadFolderAll(parentEntry);

    QMessageBox::information(this, "Copied Files", "All files have been successfully copied to the harddrive.");
}

void DeviceViewer::LoadDrives()
{
    // clear all the items
    ui->treeWidget->clear();

    for (int i = 0; i < loadedDrives.size(); i++)
        delete loadedDrives.at(i);

    try
    {
        loadedDrives = FatxDriveDetection::GetAllFatxDrives();
        if (loadedDrives.size() < 1)
        {
            statusBar->showMessage("No drives detected", 3000);
            return;
        }

        for (int i = 0; i < loadedDrives.size(); i++)
        {
            currentDrive = loadedDrives.at(i);

            QTreeWidgetItem *driveItem = new QTreeWidgetItem(ui->treeWidget_2);
            driveItem->setText(0, "Hard Drive");
            driveItem->setIcon(0, QIcon(":/Images/harddrive.png"));

            // load the partion information
            std::vector<Partition*> parts = loadedDrives.at(i)->GetPartitions();
            for (DWORD i = 0; i < parts.size(); i++)
            {
                QTreeWidgetItem *secondItem = new QTreeWidgetItem(driveItem);
                secondItem->setText(0, QString::fromStdString(parts.at(i)->name));
                secondItem->setIcon(0, QIcon(":/Images/partition.png"));
                secondItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
                secondItem->setData(0, Qt::UserRole, QVariant::fromValue(parts.at(i)));
                secondItem->setData(5, Qt::UserRole, QVariant::fromValue(true));
                secondItem->setData(4, Qt::UserRole, QVariant::fromValue(-1));
            }

            DrawMemoryGraph();

            // load the name of the drive
            FatxFileEntry *nameEntry = loadedDrives.at(i)->GetFileEntry("Drive:\\Content\\name.txt");
            if (nameEntry)
            {
                FatxIO nameFile = loadedDrives.at(i)->GetFatxIO(nameEntry);
                nameFile.SetPosition(0);

                // make sure that it starts with 0xFEFF
                if (nameFile.ReadWord() != 0xFEFF)
                    ui->txtDriveName->setText("Hard Drive");
                else
                    driveItem->setText(0, QString::fromStdWString(nameFile.ReadWString((nameEntry->fileSize > 0x36) ? 26 : (nameEntry->fileSize - 2) / 2)));
            }
            else
            {
                ui->txtDriveName->setText("Hard Drive");
            }

            LoadPartitions();
        }

        ui->btnPartitions->setEnabled(true);
        ui->btnSecurityBlob->setEnabled(true);
        ui->txtDriveName->setEnabled(true);
        ui->txtPath->setEnabled(true);
        ui->treeWidget->setEnabled(true);
        ui->treeWidget_2->setEnabled(true);
        ui->lblFeeMemory->setEnabled(true);
        ui->lblUsedSpace->setEnabled(true);
        ui->imgFreeMem->setEnabled(true);
        ui->imgUsedMem->setEnabled(true);
        ui->imgPiechart->setEnabled(true);

        drivesLoaded = true;
        statusBar->showMessage("Drive(s) loaded successfully", 3000);
    }
    catch (std::string error)
    {
        QMessageBox::warning(this, "Problem Loading", "The drive failed to load.\n\n" + QString::fromStdString(error));
    }
}

void DeviceViewer::on_treeWidget_doubleClicked(const QModelIndex &index)
{
    try
    {
        // get the item
        QTreeWidgetItem *item = (QTreeWidgetItem*)index.internalPointer();

        // set the current parent
        FatxFileEntry *currentParent = GetFatxFileEntry(item);

        if ((currentParent->fileAttributes & FatxDirectory) == 0)
            return;

        parentEntry = currentParent;

        LoadFolderAll(currentParent);
    }
    catch (std::string error)
    {
        QMessageBox::warning(this, "Problem Loading", "The folder failed to load.\n\n" + QString::fromStdString(error));
    }
}

FatxFileEntry* DeviceViewer::GetFatxFileEntry(QTreeWidgetItem *item)
{
    FatxFileEntry *currentParent;
    if (item->data(5, Qt::UserRole).toBool())
    {
        Partition *part = item->data(0, Qt::UserRole).value<Partition*>();
        currentParent = &part->root;
    }
    else
        currentParent = item->data(0, Qt::UserRole).value<FatxFileEntry*>();

    return currentParent;
}

void DeviceViewer::LoadFolderAll(FatxFileEntry *folder)
{
    try
    {
        if (directoryChain.at(directoryChain.size() - 1) != folder)
            directoryChain.push_back(folder);

        ui->btnBack->setEnabled(directoryChain.size() > 1);

        progressBar->setVisible(true);
        progressBar->setMaximum(0);

        ui->treeWidget->clear();
        currentDrive = folder->partition->drive;
        currentDrive->GetChildFileEntries(folder, updateUI);

        for (int i = 0; i < folder->cachedFiles.size(); i++)
        {
            // get the entry
            FatxFileEntry *entry = &folder->cachedFiles.at(i);

            // don't show if it's deleted
            if (entry->nameLen == FATX_ENTRY_DELETED)
                continue;

            // setup the tree widget item
            QTreeWidgetItem *entryItem = new QTreeWidgetItem(ui->treeWidget);
            entryItem->setData(0, Qt::UserRole, QVariant::fromValue(entry));
            entryItem->setData(5, Qt::UserRole, QVariant(false));

            // show the indicator if it's a directory
            if (entry->fileAttributes & FatxDirectory)
                entryItem->setIcon(0, QIcon(":/Images/FolderFileIcon.png"));
            else
            {
                QIcon fileIcon;

                currentDrive->GetFileEntryMagic(entry);

                QtHelpers::GetFileIcon(entry->magic, QString::fromStdString(entry->name), fileIcon, *entryItem);

                entryItem->setIcon(0, fileIcon);
                entryItem->setText(1, QString::fromStdString(ByteSizeToString(entry->fileSize)));
            }

            // setup the text
            entryItem->setText(0, QString::fromStdString(entry->name));

            MSTime createdtime = DWORDToMSTime(entry->creationDate);

            QDate date;
            date.setDate(createdtime.year, createdtime.month, createdtime.monthDay);

            entryItem->setText(2, date.toString(Qt::DefaultLocaleShortDate));

            if (i % 25 == 0)
                QApplication::processEvents();
        }

        progressBar->setVisible(false);
        progressBar->setMaximum(1);

        ui->txtPath->setText(QString::fromStdString(folder->path + folder->name + "\\"));
    }
    catch (std::string error)
    {
        QMessageBox::warning(this, "Problem Loading", "The folder failed to load.\n\n" + QString::fromStdString(error));
    }
}

void DeviceViewer::LoadFolderTree(QTreeWidgetItem *item)
{
    try
    {
        // get the FatxFile entry and make sure it's a directory
        FatxFileEntry *folder = GetFatxFileEntry(item);
        if ((folder->fileAttributes & FatxDirectory) == 0)
            return;

        currentDrive->GetChildFileEntries(folder);

        for (DWORD i = 0; i < folder->cachedFiles.size(); i++)
        {
            // if it isn't a folder then don't bother loading it
            FatxFileEntry *entry = &folder->cachedFiles.at(i);
            if ((entry->fileAttributes & FatxDirectory) == 0 || entry->nameLen == FATX_ENTRY_DELETED)
                continue;

            QTreeWidgetItem *subFolder = new QTreeWidgetItem(item);
            subFolder->setIcon(0, QIcon(":/Images/FolderFileIcon.png"));
            subFolder->setText(0, QString::fromStdString(entry->name));

            subFolder->setData(0, Qt::UserRole, QVariant::fromValue(entry));
            subFolder->setData(4, Qt::UserRole, QVariant(item->data(4, Qt::UserRole).toInt() + 1));
            subFolder->setData(5, Qt::UserRole, QVariant::fromValue(false));

            subFolder->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }

        if (item->childCount() == 0)
            item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
    }
    catch (std::string error)
    {
        QMessageBox::warning(this, "Problem Loading", "The folder failed to load.\n\n" + QString::fromStdString(error));
    }
}

void DeviceViewer::on_btnBack_clicked()
{
    int index = directoryChain.size() - 2;
    FatxFileEntry *entry = directoryChain.at(index);
    parentEntry = entry;

    if (entry->name == "Drive Root")
        LoadPartitions();
    else
        LoadFolderAll(entry);
    directoryChain.removeLast();
    directoryChain.removeLast();

    ui->btnBack->setEnabled(directoryChain.size() > 1);
}

void DeviceViewer::LoadPartitions()
{
    ui->treeWidget->clear();

    ui->txtPath->setText("Drive:\\");

    // load partitions
    std::vector<Partition*> parts = currentDrive->GetPartitions();
    for (int i = 0; i < parts.size(); i++)
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidget);
        item->setData(5, Qt::UserRole, QVariant(true));
        item->setIcon(0, QIcon(":/Images/partition.png"));

        item->setText(0, QString::fromStdString(parts.at(i)->name));
        item->setText(1, QString::fromStdString(ByteSizeToString(parts.at(i)->size)));
        item->setData(0, Qt::UserRole, QVariant::fromValue(parts.at(i)));
    }

    FatxFileEntry *entry = new FatxFileEntry;
    entry->name = "Drive Root";
    directoryChain.push_back(entry);
}

void DeviceViewer::GetSubFiles(FatxFileEntry *parent, QList<void *> &entries)
{
    if ((parent->fileAttributes & FatxDirectory) == 0)
    {
        entries.push_back(parent);
        return;
    }

    currentDrive->GetChildFileEntries(parent);

    for (DWORD i = 0; i < parent->cachedFiles.size(); i++)
        if (parent->cachedFiles.at(i).nameLen != FATX_ENTRY_DELETED)
            GetSubFiles(&parent->cachedFiles.at(i), entries);
}

void DeviceViewer::on_treeWidget_2_itemExpanded(QTreeWidgetItem *item)
{
    if (item->childCount() > 0)
        return;

    LoadFolderTree(item);
}

void DeviceViewer::on_treeWidget_2_itemClicked(QTreeWidgetItem *item, int column)
{
    if (!item->parent())
        return;

    FatxFileEntry *entry = GetFatxFileEntry(item);
    LoadFolderAll(entry);
}

void DeviceViewer::on_btnSecurityBlob_clicked()
{
    SecuritySectorDialog dialog(currentDrive, this);
    dialog.exec();
}

void DeviceViewer::on_btnPartitions_clicked()
{
    PartitionDialog dialog(currentDrive->GetPartitions(), this);
    dialog.exec();
}

void DeviceViewer::onDragEntered(QDragEnterEvent *event)
{
    if (parentEntry != NULL && parentEntry->name != "Drive Root" && event->mimeData()->hasFormat("text/uri-list"))
    {
        event->acceptProposedAction();
        statusBar->showMessage("Copy file(s) here");
    }
}

void DeviceViewer::onDragDropped(QDropEvent *event)
{
    statusBar->showMessage("");

    QList<QUrl> filePaths = event->mimeData()->urls();

    // fix the file name to remove the "file:///" at the beginning
    QList<void*> files;
    for (int i = 0; i < filePaths.size(); i++)
        files.push_back(new QString(filePaths.at(i).toString().mid(8)));

    InjectFiles(files);
}

void DeviceViewer::onDragLeft(QDragLeaveEvent *event)
{
    statusBar->showMessage("");
}

void updateUI(void *arg, bool finished)
{
    QApplication::processEvents();
}
