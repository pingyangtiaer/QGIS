# -*- coding: utf-8 -*-

"""
/***************************************************************************
Name                 : DB Manager
Description          : Database manager plugin for QGIS
Date                 : May 23, 2011
copyright            : (C) 2011 by Giuseppe Sucameli
email                : brush.tyler@gmail.com

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
"""

# this will disable the dbplugin if the connector raise an ImportError
from .connector import PostGisDBConnector

from qgis.PyQt.QtCore import QSettings, Qt, QRegExp
from qgis.PyQt.QtGui import QIcon
from qgis.PyQt.QtWidgets import QAction, QApplication, QMessageBox
from qgis.gui import QgsMessageBar

from ..plugin import ConnectionError, InvalidDataException, DBPlugin, Database, Schema, Table, VectorTable, RasterTable, \
    TableField, TableConstraint, TableIndex, TableTrigger, TableRule

import re

from . import resources_rc  # NOQA


def classFactory():
    return PostGisDBPlugin


class PostGisDBPlugin(DBPlugin):

    @classmethod
    def icon(self):
        return QIcon(":/db_manager/postgis/icon")

    @classmethod
    def typeName(self):
        return 'postgis'

    @classmethod
    def typeNameString(self):
        return 'PostGIS'

    @classmethod
    def providerName(self):
        return 'postgres'

    @classmethod
    def connectionSettingsKey(self):
        return '/PostgreSQL/connections'

    def databasesFactory(self, connection, uri):
        return PGDatabase(connection, uri)

    def connect(self, parent=None):
        conn_name = self.connectionName()
        settings = QSettings()
        settings.beginGroup(u"/%s/%s" % (self.connectionSettingsKey(), conn_name))

        if not settings.contains("database"):  # non-existent entry?
            raise InvalidDataException(self.tr('There is no defined database connection "%s".') % conn_name)

        from qgis.core import QgsDataSourceURI

        uri = QgsDataSourceURI()

        settingsList = ["service", "host", "port", "database", "username", "password", "authcfg"]
        service, host, port, database, username, password, authcfg = [settings.value(x, "", type=str) for x in settingsList]

        useEstimatedMetadata = settings.value("estimatedMetadata", False, type=bool)
        sslmode = settings.value("sslmode", QgsDataSourceURI.SSLprefer, type=int)

        settings.endGroup()

        if service:
            uri.setConnection(service, database, username, password, sslmode, authcfg)
        else:
            uri.setConnection(host, port, database, username, password, sslmode, authcfg)

        uri.setUseEstimatedMetadata(useEstimatedMetadata)

        try:
            return self.connectToUri(uri)
        except ConnectionError:
            return False


class PGDatabase(Database):

    def __init__(self, connection, uri):
        Database.__init__(self, connection, uri)

    def connectorsFactory(self, uri):
        return PostGisDBConnector(uri)

    def dataTablesFactory(self, row, db, schema=None):
        return PGTable(row, db, schema)

    def vectorTablesFactory(self, row, db, schema=None):
        return PGVectorTable(row, db, schema)

    def rasterTablesFactory(self, row, db, schema=None):
        return PGRasterTable(row, db, schema)

    def schemasFactory(self, row, db):
        return PGSchema(row, db)

    def sqlResultModel(self, sql, parent):
        from .data_model import PGSqlResultModel

        return PGSqlResultModel(self, sql, parent)

    def registerDatabaseActions(self, mainWindow):
        Database.registerDatabaseActions(self, mainWindow)

        # add a separator
        separator = QAction(self)
        separator.setSeparator(True)
        mainWindow.registerAction(separator, self.tr("&Table"))

        action = QAction(self.tr("Run &Vacuum Analyze"), self)
        mainWindow.registerAction(action, self.tr("&Table"), self.runVacuumAnalyzeActionSlot)

    def runVacuumAnalyzeActionSlot(self, item, action, parent):
        QApplication.restoreOverrideCursor()
        try:
            if not isinstance(item, Table) or item.isView:
                parent.infoBar.pushMessage(self.tr("Select a table for vacuum analyze."), QgsMessageBar.INFO,
                                           parent.iface.messageTimeout())
                return
        finally:
            QApplication.setOverrideCursor(Qt.WaitCursor)

        item.runVacuumAnalyze()


class PGSchema(Schema):

    def __init__(self, row, db):
        Schema.__init__(self, db)
        self.oid, self.name, self.owner, self.perms, self.comment = row


class PGTable(Table):

    def __init__(self, row, db, schema=None):
        Table.__init__(self, db, schema)
        self.name, schema_name, self._relationType, self.owner, self.estimatedRowCount, self.pages, self.comment = row
        self.isView = self._relationType in set(['v', 'm'])
        self.estimatedRowCount = int(self.estimatedRowCount)

    def runVacuumAnalyze(self):
        self.aboutToChange.emit()
        self.database().connector.runVacuumAnalyze((self.schemaName(), self.name))
        # TODO: change only this item, not re-create all the tables in the schema/database
        self.schema().refresh() if self.schema() else self.database().refresh()

    def runAction(self, action):
        action = unicode(action)

        if action.startswith("vacuumanalyze/"):
            if action == "vacuumanalyze/run":
                self.runVacuumAnalyze()
                return True

        elif action.startswith("rule/"):
            parts = action.split('/')
            rule_name = parts[1]
            rule_action = parts[2]

            msg = u"Do you want to %s rule %s?" % (rule_action, rule_name)

            QApplication.restoreOverrideCursor()

            try:
                if QMessageBox.question(None, self.tr("Table rule"), msg,
                                        QMessageBox.Yes | QMessageBox.No) == QMessageBox.No:
                    return False
            finally:
                QApplication.setOverrideCursor(Qt.WaitCursor)

            if rule_action == "delete":
                self.aboutToChange.emit()
                self.database().connector.deleteTableRule(rule_name, (self.schemaName(), self.name))
                self.refreshRules()
                return True

        return Table.runAction(self, action)

    def tableFieldsFactory(self, row, table):
        return PGTableField(row, table)

    def tableConstraintsFactory(self, row, table):
        return PGTableConstraint(row, table)

    def tableIndexesFactory(self, row, table):
        return PGTableIndex(row, table)

    def tableTriggersFactory(self, row, table):
        return PGTableTrigger(row, table)

    def tableRulesFactory(self, row, table):
        return PGTableRule(row, table)

    def info(self):
        from .info_model import PGTableInfo

        return PGTableInfo(self)

    def tableDataModel(self, parent):
        from .data_model import PGTableDataModel

        return PGTableDataModel(self, parent)

    def delete(self):
        self.aboutToChange.emit()
        if self.isView:
            ret = self.database().connector.deleteView((self.schemaName(), self.name), self._relationType == 'm')
        else:
            ret = self.database().connector.deleteTable((self.schemaName(), self.name))
        if not ret:
            self.deleted.emit()
        return ret


class PGVectorTable(PGTable, VectorTable):

    def __init__(self, row, db, schema=None):
        PGTable.__init__(self, row[:-4], db, schema)
        VectorTable.__init__(self, db, schema)
        self.geomColumn, self.geomType, self.geomDim, self.srid = row[-4:]

    def info(self):
        from .info_model import PGVectorTableInfo

        return PGVectorTableInfo(self)

    def runAction(self, action):
        if PGTable.runAction(self, action):
            return True
        return VectorTable.runAction(self, action)


class PGRasterTable(PGTable, RasterTable):

    def __init__(self, row, db, schema=None):
        PGTable.__init__(self, row[:-6], db, schema)
        RasterTable.__init__(self, db, schema)
        self.geomColumn, self.pixelType, self.pixelSizeX, self.pixelSizeY, self.isExternal, self.srid = row[-6:]
        self.geomType = 'RASTER'

    def info(self):
        from .info_model import PGRasterTableInfo

        return PGRasterTableInfo(self)

    def gdalUri(self, uri=None):
        if not uri:
            uri = self.database().uri()
        schema = (u'schema=%s' % self.schemaName()) if self.schemaName() else ''
        dbname = (u'dbname=%s' % uri.database()) if uri.database() else ''
        host = (u'host=%s' % uri.host()) if uri.host() else ''
        user = (u'user=%s' % uri.username()) if uri.username() else ''
        passw = (u'password=%s' % uri.password()) if uri.password() else ''
        port = (u'port=%s' % uri.port()) if uri.port() else ''

        # Find first raster field
        col = ''
        for fld in self.fields():
            if fld.dataType == "raster":
                col = u'column=%s' % fld.name
                break

        gdalUri = u'PG: %s %s %s %s %s mode=2 %s %s table=%s' % \
                  (dbname, host, user, passw, port, schema, col, self.name)

        return gdalUri

    def mimeUri(self):
        # QGIS has no provider for PGRasters, let's use GDAL
        uri = u"raster:gdal:%s:%s" % (self.name, re.sub(":", "\:", self.gdalUri()))
        return uri

    def toMapLayer(self):
        from qgis.core import QgsRasterLayer, QgsContrastEnhancement, QgsDataSourceURI, QgsCredentials

        rl = QgsRasterLayer(self.gdalUri(), self.name)
        if not rl.isValid():
            err = rl.error().summary()
            uri = QgsDataSourceURI(self.database().uri())
            conninfo = uri.connectionInfo(False)
            username = uri.username()
            password = uri.password()

            for i in range(3):
                (ok, username, password) = QgsCredentials.instance().get(conninfo, username, password, err)
                if ok:
                    uri.setUsername(username)
                    uri.setPassword(password)
                    rl = QgsRasterLayer(self.gdalUri(uri), self.name)
                    if rl.isValid():
                        break

        if rl.isValid():
            rl.setContrastEnhancement(QgsContrastEnhancement.StretchToMinimumMaximum)
        return rl


class PGTableField(TableField):

    def __init__(self, row, table):
        TableField.__init__(self, table)
        self.num, self.name, self.dataType, self.charMaxLen, self.modifier, self.notNull, self.hasDefault, self.default, typeStr = row
        self.primaryKey = False

        # get modifier (e.g. "precision,scale") from formatted type string
        trimmedTypeStr = typeStr.strip()
        regex = QRegExp("\((.+)\)$")
        startpos = regex.indexIn(trimmedTypeStr)
        if startpos >= 0:
            self.modifier = regex.cap(1).strip()
        else:
            self.modifier = None

        # find out whether fields are part of primary key
        for con in self.table().constraints():
            if con.type == TableConstraint.TypePrimaryKey and self.num in con.columns:
                self.primaryKey = True
                break


class PGTableConstraint(TableConstraint):

    def __init__(self, row, table):
        TableConstraint.__init__(self, table)
        self.name, constr_type_str, self.isDefferable, self.isDeffered, columns = row[:5]
        self.columns = map(int, columns.split(' '))

        if constr_type_str in TableConstraint.types:
            self.type = TableConstraint.types[constr_type_str]
        else:
            self.type = TableConstraint.TypeUnknown

        if self.type == TableConstraint.TypeCheck:
            self.checkSource = row[5]
        elif self.type == TableConstraint.TypeForeignKey:
            self.foreignTable = row[6]
            self.foreignOnUpdate = TableConstraint.onAction[row[7]]
            self.foreignOnDelete = TableConstraint.onAction[row[8]]
            self.foreignMatchType = TableConstraint.matchTypes[row[9]]
            self.foreignKeys = row[10]


class PGTableIndex(TableIndex):

    def __init__(self, row, table):
        TableIndex.__init__(self, table)
        self.name, columns, self.isUnique = row
        self.columns = map(int, columns.split(' '))


class PGTableTrigger(TableTrigger):

    def __init__(self, row, table):
        TableTrigger.__init__(self, table)
        self.name, self.function, self.type, self.enabled = row


class PGTableRule(TableRule):

    def __init__(self, row, table):
        TableRule.__init__(self, table)
        self.name, self.definition = row
