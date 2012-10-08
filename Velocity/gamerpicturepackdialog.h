#ifndef GAMERPICTUREPACKDIALOG_H
#define GAMERPICTUREPACKDIALOG_H

// qt
#include <QDialog>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QList>
#include <QStatusBar>
#include <QMenu>
#include <QFileDialog>
#include <QBuffer>
#include "qthelpers.h"

// xbox360
#include "Stfs/StfsPackage.h"

// other
#include "titleidfinder.h"
#include "gamerpicturedownloader.h"

namespace Ui {
class GamerPicturePackDialog;
}

class GamerPicturePackDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit GamerPicturePackDialog(QStatusBar *statusBar, QWidget *parent = 0);
    ~GamerPicturePackDialog();
    
public slots:
    void gamercardNetworkReply(QNetworkReply*);

private slots:
    void on_comboBox_currentIndexChanged(const QString &arg1);
    void on_pushButton_clicked();
    void onTitleIDSearchReturn(QList<struct Title>);
    void on_listGameNames_itemClicked(QListWidgetItem *item);
    void gamerPictureDownloaded(QImage, QString);
    void on_listSearch_itemDoubleClicked(QListWidgetItem *item);
    void on_listPack_itemDoubleClicked(QListWidgetItem *item);
    void showContextMenuSearch(QPoint p);
    void showContextMenuPack(QPoint p);
    void on_txtPackName_textChanged(const QString &arg1);
    void on_btnCreatePack_clicked();
    void on_txtSearch_textChanged(const QString &arg1);

private:
    Ui::GamerPicturePackDialog *ui;
    TitleIdFinder *titleIDFinder;

    QList<struct Title> *currentTitles;
    QList<QString> *searchedIDs;
    QList<QString> *addedIDs;
    QList<QString> searchedGamertags;
    QList<DWORD> *searchedTitleIDs;
    QStatusBar *statusBar;
    QNetworkAccessManager *manager;
    GamerPictureDownloader *downloader;
    bool requestImage;

    void findGamerPictures(QString titleID);
    void switchImage(QListWidgetItem *currentItem, QListWidget *current, QListWidget *toAddTo, QList<QString> *currentStrs, QList<QString> *toAddToStrs);
    QString getImageName(QString id, bool big);
    void showContextMenu(QPoint p, QListWidget *current, QListWidget *toAddTo, QList<QString> *currentStrs, QList<QString> *toAddToStrs);
    bool verifyGamertag(QString gamertag);
};

#endif // GAMERPICTUREPACKDIALOG_H