#include "download_manager.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QTextStream>

namespace dm{

namespace net{

download_manager::download_manager(QObject *obj) :
    QObject(obj),
    manager_{new QNetworkAccessManager(obj)},
    max_download_size_{4},
    total_download_files_{0},
    uuid_{0}
{

}

int_fast64_t download_manager::
append(QUrl const &value,
       QString const &save_at,
       QString const &save_as)
{
    auto *reply = start_download_impl(value);
    if(reply){
        auto &uid_index = download_info_.get<uid>();
        if(uid_index.insert({uuid_, reply,
                            save_at, save_as}).second)
        {
            return uuid_++;
        }
    }else{
        qDebug()<<this<<" cannot start to download";
    }

    return -1;
}

size_t download_manager::get_max_download_size() const
{
    return max_download_size_;
}

size_t download_manager::get_total_download_file() const
{
    return total_download_files_;
}

void download_manager::set_max_download_size(size_t value)
{
    max_download_size_ = value;
}

bool download_manager::start_download(const QUrl &value)
{
    return start_download_impl(value) != nullptr;
}

QNetworkReply* download_manager::start_download_impl(QUrl const &value)
{     
    if(total_download_files_ < max_download_size_){
        QNetworkRequest request(value);
        auto *current_download = manager_->get(request);
        connect(current_download, SIGNAL(downloadProgress(qint64,qint64)),
                SLOT(download_progress(qint64,qint64)));
        connect(current_download, SIGNAL(finished()),
                SLOT(download_finished()));
        connect(current_download, SIGNAL(readyRead()),
                SLOT(download_ready_read()));
        emit download_size_changed(++total_download_files_);

        return current_download;
    }else{
        qDebug()<<this<<" max_download_size_ >= total_download_files_";
    }

    return nullptr;
}

void download_manager::download_finished()
{
    class recycle
    {
    public:
        explicit recycle(QNetworkReply *value) :
            value_(value) {}
        ~recycle(){ value_->deleteLater(); }

    private:
        QNetworkReply *value_;
    };

    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if(reply){
        recycle rc(reply);

        auto &net_index = download_info_.get<net_reply>();
        auto it = net_index.find(reply);
        if(it != std::end(net_index)){
            auto const data = reply->readAll();
            save_data(*it, data);
            auto const uuid = it->uuid_;
            net_index.erase(it);
            emit download_finished(uuid);
            emit download_size_changed(--total_download_files_);
        }
    }else{
        qDebug()<<__func__<<" : do not exist";
    }
}

void download_manager::
download_progress(qint64 bytes_received,
                  qint64 bytes_total)
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if(reply){
        auto &net_index = download_info_.get<net_reply>();
        auto it = net_index.find(reply);
        qDebug()<<__func__<< " receive "<<bytes_received;
        qDebug()<<__func__<< " total "<<bytes_total;
        if(it != std::end(net_index)){
            emit download_progress(it->uuid_, bytes_received,
                                   bytes_total);
        }
    }
}

void download_manager::download_ready_read()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if(reply){
        qDebug()<<__func__<<" ready read";
        auto &net_index = download_info_.get<net_reply>();
        auto it = net_index.find(reply);
        if(it != std::end(net_index)){
            emit download_ready_read(it->uuid_);
        }
    }else{
        qDebug()<<__func__<< " cannot cast sender to reply";
    }
}

void download_manager::
save_data(download_info const &info,
          QByteArray const &data)
{
    QDir dir(info.save_at_);
    if(!dir.exists()){
        if(!dir.mkpath(info.save_at_)){
            QMessageBox::warning(0, tr("Warning"),
                                 tr("Can not create directory"));
        }
    }
    QFile file(info.save_at_ + "/" + info.save_as_);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)){
        qDebug()<<__func__<<" cannot open file";
        return;
    }

    QTextStream out(&file);
    out<<data;
    qDebug()<<__func__<<"save buffer";
}

}

}
