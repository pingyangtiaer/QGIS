# -*- coding: utf-8 -*-

"""
***************************************************************************
    test_qgsarrowsymbollayer.py
    ---------------------
    Date                 : March 2016
    Copyright            : (C) 2016 by Hugo Mercier
    Email                : hugo dot mercier at oslandia dot com
***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************
"""

__author__ = 'Hugo Mercier'
__date__ = 'March 2016'
__copyright__ = '(C) 2016, Hugo Mercier'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

import qgis  # NOQA

import os

from qgis.PyQt.QtCore import QSize
from qgis.PyQt.QtGui import QColor

from qgis.core import (
    QgsVectorLayer,
    QgsSingleSymbolRendererV2,
    QgsLineSymbolV2,
    QgsFillSymbolV2,
    QgsMapLayerRegistry,
    QgsRectangle,
    QgsArrowSymbolLayer,
    QgsSymbolV2,
    QgsMultiRenderChecker,
    QgsDataDefined
)

from qgis.testing import start_app, unittest
from qgis.testing.mocked import get_iface
from utilities import unitTestDataPath

# Convenience instances in case you may need them
# not used in this test
start_app()
TEST_DATA_DIR = unitTestDataPath()


class TestQgsArrowSymbolLayer(unittest.TestCase):

    def setUp(self):
        self.iface = get_iface()

        lines_shp = os.path.join(TEST_DATA_DIR, 'lines.shp')
        self.lines_layer = QgsVectorLayer(lines_shp, 'Lines', 'ogr')
        QgsMapLayerRegistry.instance().addMapLayer(self.lines_layer)

        # Create style
        sym2 = QgsLineSymbolV2.createSimple({'color': '#fdbf6f'})
        self.lines_layer.setRendererV2(QgsSingleSymbolRendererV2(sym2))

        self.mapsettings = self.iface.mapCanvas().mapSettings()
        self.mapsettings.setOutputSize(QSize(400, 400))
        self.mapsettings.setOutputDpi(96)
        self.mapsettings.setExtent(QgsRectangle(-113, 28, -91, 40))
        self.mapsettings.setBackgroundColor(QColor("white"))

    def tearDown(self):
        QgsMapLayerRegistry.instance().removeAllMapLayers()

    def test_1(self):
        sym = self.lines_layer.rendererV2().symbol()
        sym_layer = QgsArrowSymbolLayer.create({'head_size': '6.5'})
        dd = QgsDataDefined("(@geometry_point_num % 4) * 2")
        sym_layer.setDataDefinedProperty("arrow_width", dd)
        dd2 = QgsDataDefined("(@geometry_point_num % 4) * 2")
        sym_layer.setDataDefinedProperty("head_size", dd2)
        fill_sym = QgsFillSymbolV2.createSimple({'color': '#8bcfff', 'outline_color': '#000000', 'outline_style': 'solid', 'outline_width': '1'})
        sym_layer.setSubSymbol(fill_sym)
        sym.changeSymbolLayer(0, sym_layer)

        rendered_layers = [self.lines_layer.id()]
        self.mapsettings.setLayers(rendered_layers)

        renderchecker = QgsMultiRenderChecker()
        renderchecker.setMapSettings(self.mapsettings)
        renderchecker.setControlName('expected_arrowsymbollayer_1')
        self.assertTrue(renderchecker.runTest('arrowsymbollayer_1'))

    def test_2(self):
        sym = self.lines_layer.rendererV2().symbol()
        # double headed
        sym_layer = QgsArrowSymbolLayer.create({'arrow_width': '5', 'head_size': '6.5', 'head_type': '2'})
        fill_sym = QgsFillSymbolV2.createSimple({'color': '#8bcfff', 'outline_color': '#000000', 'outline_style': 'solid', 'outline_width': '1'})
        sym_layer.setSubSymbol(fill_sym)
        sym.changeSymbolLayer(0, sym_layer)

        rendered_layers = [self.lines_layer.id()]
        self.mapsettings.setLayers(rendered_layers)

        renderchecker = QgsMultiRenderChecker()
        renderchecker.setMapSettings(self.mapsettings)
        renderchecker.setControlName('expected_arrowsymbollayer_2')
        self.assertTrue(renderchecker.runTest('arrowsymbollayer_2'))

if __name__ == '__main__':
    unittest.main()
