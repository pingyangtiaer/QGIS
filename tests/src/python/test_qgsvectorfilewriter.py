# -*- coding: utf-8 -*-
"""QGIS Unit tests for QgsVectorFileWriter.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Tim Sutton'
__date__ = '20/08/2012'
__copyright__ = 'Copyright 2012, The QGIS Project'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

import qgis  # NOQA

from qgis.core import (QgsVectorLayer,
                       QgsFeature,
                       QgsGeometry,
                       QgsPoint,
                       QgsCoordinateReferenceSystem,
                       QgsVectorFileWriter,
                       QgsFeatureRequest,
                       QgsWKBTypes
                       )
from qgis.PyQt.QtCore import QDate, QTime, QDateTime, QVariant, QDir
import os
import osgeo.gdal
import platform
from qgis.testing import start_app, unittest
from utilities import writeShape, compareWkt

start_app()


class TestQgsVectorLayer(unittest.TestCase):

    mMemoryLayer = None

    def testWrite(self):
        """Check we can write a vector file."""
        self.mMemoryLayer = QgsVectorLayer(
            ('Point?crs=epsg:4326&field=name:string(20)&'
             'field=age:integer&field=size:double&index=yes'),
            'test',
            'memory')

        assert self.mMemoryLayer is not None, 'Provider not initialized'
        myProvider = self.mMemoryLayer.dataProvider()
        assert myProvider is not None

        ft = QgsFeature()
        ft.setGeometry(QgsGeometry.fromPoint(QgsPoint(10, 10)))
        ft.setAttributes(['Johny', 20, 0.3])
        myResult, myFeatures = myProvider.addFeatures([ft])
        assert myResult
        assert len(myFeatures) > 0

        writeShape(self.mMemoryLayer, 'writetest.shp')

    def testDateTimeWriteShapefile(self):
        """Check writing date and time fields to an ESRI shapefile."""
        ml = QgsVectorLayer(
            ('Point?crs=epsg:4326&field=id:int&'
             'field=date_f:date&field=time_f:time&field=dt_f:datetime'),
            'test',
            'memory')

        assert ml is not None, 'Provider not initialized'
        assert ml.isValid(), 'Source layer not valid'
        provider = ml.dataProvider()
        assert provider is not None

        ft = QgsFeature()
        ft.setGeometry(QgsGeometry.fromPoint(QgsPoint(10, 10)))
        ft.setAttributes([1, QDate(2014, 3, 5), QTime(13, 45, 22), QDateTime(QDate(2014, 3, 5), QTime(13, 45, 22))])
        res, features = provider.addFeatures([ft])
        assert res
        assert len(features) > 0

        dest_file_name = os.path.join(str(QDir.tempPath()), 'datetime.shp')
        crs = QgsCoordinateReferenceSystem()
        crs.createFromId(4326, QgsCoordinateReferenceSystem.EpsgCrsId)
        write_result = QgsVectorFileWriter.writeAsVectorFormat(
            ml,
            dest_file_name,
            'utf-8',
            crs,
            'ESRI Shapefile')
        self.assertEqual(write_result, QgsVectorFileWriter.NoError)

        # Open result and check
        created_layer = QgsVectorLayer(u'{}|layerid=0'.format(dest_file_name), u'test', u'ogr')

        fields = created_layer.dataProvider().fields()
        self.assertEqual(fields.at(fields.indexFromName('date_f')).type(), QVariant.Date)
        #shapefiles do not support time types, result should be string
        self.assertEqual(fields.at(fields.indexFromName('time_f')).type(), QVariant.String)
        #shapefiles do not support datetime types, result should be string
        self.assertEqual(fields.at(fields.indexFromName('dt_f')).type(), QVariant.String)

        f = next(created_layer.getFeatures(QgsFeatureRequest()))

        date_idx = created_layer.fieldNameIndex('date_f')
        assert isinstance(f.attributes()[date_idx], QDate)
        self.assertEqual(f.attributes()[date_idx], QDate(2014, 3, 5))
        time_idx = created_layer.fieldNameIndex('time_f')
        #shapefiles do not support time types
        assert isinstance(f.attributes()[time_idx], basestring)
        self.assertEqual(f.attributes()[time_idx], '13:45:22')
        #shapefiles do not support datetime types
        datetime_idx = created_layer.fieldNameIndex('dt_f')
        assert isinstance(f.attributes()[datetime_idx], basestring)
        self.assertEqual(f.attributes()[datetime_idx], QDateTime(QDate(2014, 3, 5), QTime(13, 45, 22)).toString("yyyy/MM/dd hh:mm:ss.zzz"))

    def testDateTimeWriteTabfile(self):
        """Check writing date and time fields to an MapInfo tabfile."""
        ml = QgsVectorLayer(
            ('Point?crs=epsg:4326&field=id:int&'
             'field=date_f:date&field=time_f:time&field=dt_f:datetime'),
            'test',
            'memory')

        assert ml is not None, 'Provider not initialized'
        assert ml.isValid(), 'Source layer not valid'
        provider = ml.dataProvider()
        assert provider is not None

        ft = QgsFeature()
        ft.setGeometry(QgsGeometry.fromPoint(QgsPoint(10, 10)))
        ft.setAttributes([1, QDate(2014, 3, 5), QTime(13, 45, 22), QDateTime(QDate(2014, 3, 5), QTime(13, 45, 22))])
        res, features = provider.addFeatures([ft])
        assert res
        assert len(features) > 0

        dest_file_name = os.path.join(str(QDir.tempPath()), 'datetime.tab')
        crs = QgsCoordinateReferenceSystem()
        crs.createFromId(4326, QgsCoordinateReferenceSystem.EpsgCrsId)
        write_result = QgsVectorFileWriter.writeAsVectorFormat(
            ml,
            dest_file_name,
            'utf-8',
            crs,
            'MapInfo File')
        self.assertEqual(write_result, QgsVectorFileWriter.NoError)

        # Open result and check
        created_layer = QgsVectorLayer(u'{}|layerid=0'.format(dest_file_name), u'test', u'ogr')

        fields = created_layer.dataProvider().fields()
        self.assertEqual(fields.at(fields.indexFromName('date_f')).type(), QVariant.Date)
        self.assertEqual(fields.at(fields.indexFromName('time_f')).type(), QVariant.Time)
        self.assertEqual(fields.at(fields.indexFromName('dt_f')).type(), QVariant.DateTime)

        f = next(created_layer.getFeatures(QgsFeatureRequest()))

        date_idx = created_layer.fieldNameIndex('date_f')
        assert isinstance(f.attributes()[date_idx], QDate)
        self.assertEqual(f.attributes()[date_idx], QDate(2014, 3, 5))
        time_idx = created_layer.fieldNameIndex('time_f')
        assert isinstance(f.attributes()[time_idx], QTime)
        self.assertEqual(f.attributes()[time_idx], QTime(13, 45, 22))
        datetime_idx = created_layer.fieldNameIndex('dt_f')
        assert isinstance(f.attributes()[datetime_idx], QDateTime)
        self.assertEqual(f.attributes()[datetime_idx], QDateTime(QDate(2014, 3, 5), QTime(13, 45, 22)))

    def testWriteShapefileWithZ(self):
        """Check writing geometries with Z dimension to an ESRI shapefile."""

        #start by saving a memory layer and forcing z
        ml = QgsVectorLayer(
            ('Point?crs=epsg:4326&field=id:int'),
            'test',
            'memory')

        assert ml is not None, 'Provider not initialized'
        assert ml.isValid(), 'Source layer not valid'
        provider = ml.dataProvider()
        assert provider is not None

        ft = QgsFeature()
        ft.setGeometry(QgsGeometry.fromWkt('PointZ (1 2 3)'))
        ft.setAttributes([1])
        res, features = provider.addFeatures([ft])
        assert res
        assert len(features) > 0

        # check with both a standard PointZ and 25d style Point25D type
        for t in [QgsWKBTypes.PointZ, QgsWKBTypes.Point25D]:
            dest_file_name = os.path.join(str(QDir.tempPath()), 'point_{}.shp'.format(QgsWKBTypes.displayString(t)))
            crs = QgsCoordinateReferenceSystem()
            crs.createFromId(4326, QgsCoordinateReferenceSystem.EpsgCrsId)
            write_result = QgsVectorFileWriter.writeAsVectorFormat(
                ml,
                dest_file_name,
                'utf-8',
                crs,
                'ESRI Shapefile',
                overrideGeometryType=t)
            self.assertEqual(write_result, QgsVectorFileWriter.NoError)

            # Open result and check
            created_layer = QgsVectorLayer(u'{}|layerid=0'.format(dest_file_name), u'test', u'ogr')
            f = next(created_layer.getFeatures(QgsFeatureRequest()))
            g = f.geometry()
            wkt = g.exportToWkt()
            expWkt = 'PointZ (1 2 3)'
            assert compareWkt(expWkt, wkt), "saving geometry with Z failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

            #also try saving out the shapefile version again, as an extra test
            #this tests that saving a layer with z WITHOUT explicitly telling the writer to keep z values,
            #will stay retain the z values
            dest_file_name = os.path.join(str(QDir.tempPath()), 'point_{}_copy.shp'.format(QgsWKBTypes.displayString(t)))
            crs = QgsCoordinateReferenceSystem()
            crs.createFromId(4326, QgsCoordinateReferenceSystem.EpsgCrsId)
            write_result = QgsVectorFileWriter.writeAsVectorFormat(
                created_layer,
                dest_file_name,
                'utf-8',
                crs,
                'ESRI Shapefile')
            self.assertEqual(write_result, QgsVectorFileWriter.NoError)

            # Open result and check
            created_layer_from_shp = QgsVectorLayer(u'{}|layerid=0'.format(dest_file_name), u'test', u'ogr')
            f = next(created_layer_from_shp.getFeatures(QgsFeatureRequest()))
            g = f.geometry()
            wkt = g.exportToWkt()
            assert compareWkt(expWkt, wkt), "saving geometry with Z failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

    def testWriteShapefileWithMultiConversion(self):
        """Check writing geometries to an ESRI shapefile with conversion to multi."""
        ml = QgsVectorLayer(
            ('Point?crs=epsg:4326&field=id:int'),
            'test',
            'memory')

        assert ml is not None, 'Provider not initialized'
        assert ml.isValid(), 'Source layer not valid'
        provider = ml.dataProvider()
        assert provider is not None

        ft = QgsFeature()
        ft.setGeometry(QgsGeometry.fromWkt('Point (1 2)'))
        ft.setAttributes([1])
        res, features = provider.addFeatures([ft])
        assert res
        assert len(features) > 0

        dest_file_name = os.path.join(str(QDir.tempPath()), 'to_multi.shp')
        crs = QgsCoordinateReferenceSystem()
        crs.createFromId(4326, QgsCoordinateReferenceSystem.EpsgCrsId)
        write_result = QgsVectorFileWriter.writeAsVectorFormat(
            ml,
            dest_file_name,
            'utf-8',
            crs,
            'ESRI Shapefile',
            forceMulti=True)
        self.assertEqual(write_result, QgsVectorFileWriter.NoError)

        # Open result and check
        created_layer = QgsVectorLayer(u'{}|layerid=0'.format(dest_file_name), u'test', u'ogr')
        f = next(created_layer.getFeatures(QgsFeatureRequest()))
        g = f.geometry()
        wkt = g.exportToWkt()
        expWkt = 'MultiPoint ((1 2))'
        assert compareWkt(expWkt, wkt), "saving geometry with multi conversion failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

    def testWriteShapefileWithAttributeSubsets(self):
        """Tests writing subsets of attributes to files."""
        ml = QgsVectorLayer(
            ('Point?crs=epsg:4326&field=id:int&field=field1:int&field=field2:int&field=field3:int'),
            'test',
            'memory')

        assert ml is not None, 'Provider not initialized'
        assert ml.isValid(), 'Source layer not valid'
        provider = ml.dataProvider()
        assert provider is not None

        ft = QgsFeature()
        ft.setGeometry(QgsGeometry.fromWkt('Point (1 2)'))
        ft.setAttributes([1, 11, 12, 13])
        res, features = provider.addFeatures([ft])
        assert res
        assert len(features) > 0

        #first write out with all attributes
        dest_file_name = os.path.join(str(QDir.tempPath()), 'all_attributes.shp')
        crs = QgsCoordinateReferenceSystem()
        crs.createFromId(4326, QgsCoordinateReferenceSystem.EpsgCrsId)
        write_result = QgsVectorFileWriter.writeAsVectorFormat(
            ml,
            dest_file_name,
            'utf-8',
            crs,
            'ESRI Shapefile',
            attributes=[])
        self.assertEqual(write_result, QgsVectorFileWriter.NoError)

        # Open result and check
        created_layer = QgsVectorLayer(u'{}|layerid=0'.format(dest_file_name), u'test', u'ogr')
        self.assertEqual(created_layer.fields().count(), 4)
        f = next(created_layer.getFeatures(QgsFeatureRequest()))
        self.assertEqual(f['id'], 1)
        self.assertEqual(f['field1'], 11)
        self.assertEqual(f['field2'], 12)
        self.assertEqual(f['field3'], 13)

        #now test writing out only a subset of attributes
        dest_file_name = os.path.join(str(QDir.tempPath()), 'subset_attributes.shp')
        write_result = QgsVectorFileWriter.writeAsVectorFormat(
            ml,
            dest_file_name,
            'utf-8',
            crs,
            'ESRI Shapefile',
            attributes=[1, 3])
        self.assertEqual(write_result, QgsVectorFileWriter.NoError)

        # Open result and check
        created_layer = QgsVectorLayer(u'{}|layerid=0'.format(dest_file_name), u'test', u'ogr')
        self.assertEqual(created_layer.fields().count(), 2)
        f = next(created_layer.getFeatures(QgsFeatureRequest()))
        self.assertEqual(f['field1'], 11)
        self.assertEqual(f['field3'], 13)

        #finally test writing no attributes
        dest_file_name = os.path.join(str(QDir.tempPath()), 'no_attributes.shp')
        write_result = QgsVectorFileWriter.writeAsVectorFormat(
            ml,
            dest_file_name,
            'utf-8',
            crs,
            'ESRI Shapefile',
            skipAttributeCreation=True)
        self.assertEqual(write_result, QgsVectorFileWriter.NoError)

        # Open result and check
        created_layer = QgsVectorLayer(u'{}|layerid=0'.format(dest_file_name), u'test', u'ogr')
        # expect only a default 'FID' field for shapefiles
        self.assertEqual(created_layer.fields().count(), 1)
        self.assertEqual(created_layer.fields()[0].name(), 'FID')
        # in this case we also check that the geometry exists, to make sure feature has been correctly written
        # even without attributes
        f = next(created_layer.getFeatures(QgsFeatureRequest()))
        g = f.geometry()
        wkt = g.exportToWkt()
        expWkt = 'Point (1 2)'
        assert compareWkt(expWkt, wkt), "geometry not saved correctly when saving without attributes : mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        self.assertEqual(f['FID'], 0)


if __name__ == '__main__':
    unittest.main()
