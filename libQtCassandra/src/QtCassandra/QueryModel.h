// Copyright (C) 2015-2016  Made to Order Software Corp.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
#pragma once

#include <controlled_vars/controlled_vars_need_init.h>

#include <QtCassandra/QCassandraQuery.h>
#include <QtCassandra/QCassandraSession.h>

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QRegExp>

#include <memory>
#include <stack>
#include <vector>

namespace QtCassandra
{


class QueryModel
    : public QAbstractItemModel
{
    Q_OBJECT

public:
    QueryModel();

    void init
        ( QCassandraSession::pointer_t session
        , const QString& keyspace_name
        , const QString& table_name
        , const QRegExp& filter = QRegExp()
        );
    void clear();

    const QString&              keyspaceName() const { return f_keyspaceName; }
    const QString&              tableName()    const { return f_tableName;    }
    QCassandraQuery::pointer_t  query()        const { return f_query;        }

    // Read only access
    //
    virtual bool			canFetchMore ( const QModelIndex & prnt ) const;
    virtual void			fetchMore    ( const QModelIndex & prnt );
    virtual Qt::ItemFlags   flags        ( QModelIndex const & index ) const;
    virtual QVariant        data         ( QModelIndex const & index, int role = Qt::DisplayRole ) const;
    virtual QModelIndex     index        ( int row, int column, const QModelIndex &parent= QModelIndex() ) const;
    virtual QModelIndex     parent       ( const QModelIndex &child ) const;
    virtual int             rowCount     ( QModelIndex const & prnt = QModelIndex() ) const;
    virtual int             columnCount  ( QModelIndex const & prnt = QModelIndex() ) const;

    virtual bool			fetchFilter     ( const QByteArray& key );
    virtual void			fetchCustomData ( QCassandraQuery::pointer_t q );

signals:
    void exceptionCaught( const QString & what, const QString & message ) const;
    void queryPageFinished() const;
    void queryFinished() const;

protected:
    QCassandraSession::pointer_t f_session;
    QString                      f_keyspaceName;
    QString                      f_tableName;
    std::vector<QByteArray>      f_rows;
    bool                         f_isMore      = false;
    int                          f_columnCount = 1;

    void doQuery      ( QCassandraQuery::pointer_t query );
    void displayError ( const std::exception & except, const QString & message ) const;

private:
    QCassandraQuery::pointer_t   f_query;
    QRegExp                      f_filter;

    void reset();
};


}
// namespace QtCassandra

// vim: ts=4 sw=4 et
