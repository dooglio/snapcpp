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

#include <QModelIndex>

#include <QtCassandra/QueryModel.h>

#include <snapwebsites/dbutils.h>

class TableModel
    : public QtCassandra::QueryModel
{
    Q_OBJECT

public:
    TableModel();

    void doQuery();

    bool sortModel() const              { return f_sortModel; }
    void setSortModel( const bool val ) { f_sortModel = val;  }

    // Read only access
    //
    virtual QVariant data( QModelIndex const & index, int role = Qt::DisplayRole ) const override;
    virtual void     fetchCustomData( QtCassandra::QCassandraQuery::pointer_t q ) override;

private:
    typedef std::map<QString,QModelIndex> sort_map_t;
    sort_map_t                      f_sortMap;
    std::shared_ptr<snap::dbutils>  f_dbutils;
    bool                            f_sortModel = false;
};


// vim: ts=4 sw=4 et