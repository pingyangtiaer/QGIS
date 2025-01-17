/***************************************************************************
 qgsmarkersymbollayerv2.h
 ---------------------
 begin                : November 2009
 copyright            : (C) 2009 by Martin Dobias
 email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMARKERSYMBOLLAYERV2_H
#define QGSMARKERSYMBOLLAYERV2_H

#include "qgssymbollayerv2.h"
#include "qgsvectorlayer.h"

#define DEFAULT_SIMPLEMARKER_NAME         "circle"
#define DEFAULT_SIMPLEMARKER_COLOR        QColor(255,0,0)
#define DEFAULT_SIMPLEMARKER_BORDERCOLOR  QColor(0,0,0)
#define DEFAULT_SIMPLEMARKER_JOINSTYLE    Qt::BevelJoin
#define DEFAULT_SIMPLEMARKER_SIZE         DEFAULT_POINT_SIZE
#define DEFAULT_SIMPLEMARKER_ANGLE        0

#include <QPen>
#include <QBrush>
#include <QPicture>
#include <QPolygonF>
#include <QFont>

class CORE_EXPORT QgsSimpleMarkerSymbolLayerV2 : public QgsMarkerSymbolLayerV2
{
  public:

    /** Constructor for QgsSimpleMarkerSymbolLayerV2.
    * @param name symbol name, should be one of "square", "rectangle", "diamond",
    * "pentagon", "hexagon", "triangle", "equilateral_triangle", "star", "arrow",
    * "circle", "cross", "cross_fill", "cross2", "line", "x", "arrowhead", "filled_arrowhead",
    * "semi_circle", "third_circle", "quarter_circle", "quarter_square", "half_square",
    * "diagonal_half_square", "right_half_triangle", "left_half_triangle"
    * @param color fill color for symbol
    * @param borderColor border color for symbol
    * @param size symbol size (in mm)
    * @param angle symbol rotation angle
    * @param scaleMethod scaling method for data defined scaling
    * @param penJoinStyle join style for outline pen
    */
    QgsSimpleMarkerSymbolLayerV2( const QString& name = DEFAULT_SIMPLEMARKER_NAME,
                                  const QColor& color = DEFAULT_SIMPLEMARKER_COLOR,
                                  const QColor& borderColor = DEFAULT_SIMPLEMARKER_BORDERCOLOR,
                                  double size = DEFAULT_SIMPLEMARKER_SIZE,
                                  double angle = DEFAULT_SIMPLEMARKER_ANGLE,
                                  QgsSymbolV2::ScaleMethod scaleMethod = DEFAULT_SCALE_METHOD,
                                  Qt::PenJoinStyle penJoinStyle = DEFAULT_SIMPLEMARKER_JOINSTYLE );

    // static stuff

    static QgsSymbolLayerV2* create( const QgsStringMap& properties = QgsStringMap() );
    static QgsSymbolLayerV2* createFromSld( QDomElement &element );

    // implemented from base classes

    QString layerType() const override;

    void startRender( QgsSymbolV2RenderContext& context ) override;

    void stopRender( QgsSymbolV2RenderContext& context ) override;

    void renderPoint( QPointF point, QgsSymbolV2RenderContext& context ) override;

    QgsStringMap properties() const override;

    QgsSimpleMarkerSymbolLayerV2* clone() const override;

    void writeSldMarker( QDomDocument &doc, QDomElement &element, const QgsStringMap& props ) const override;

    QString ogrFeatureStyle( double mmScaleFactor, double mapUnitScaleFactor ) const override;

    QString name() const { return mName; }
    void setName( const QString& name ) { mName = name; }

    QColor borderColor() const { return mBorderColor; }
    void setBorderColor( const QColor& color ) { mBorderColor = color; }

    /** Get outline join style.
     * @note added in 2.4 */
    Qt::PenStyle outlineStyle() const { return mOutlineStyle; }
    /** Set outline join style.
     * @note added in 2.4 */
    void setOutlineStyle( Qt::PenStyle outlineStyle ) { mOutlineStyle = outlineStyle; }

    /** Get outline join style.
     * @note added in 2.16 */
    Qt::PenJoinStyle penJoinStyle() const { return mPenJoinStyle; }
    /** Set outline join style.
     * @note added in 2.16 */
    void setPenJoinStyle( Qt::PenJoinStyle style ) { mPenJoinStyle = style; }

    /** Get outline color.
     * @note added in 2.1 */
    QColor outlineColor() const override { return borderColor(); }
    /** Set outline color.
     * @note added in 2.1 */
    void setOutlineColor( const QColor& color ) override { setBorderColor( color ); }

    /** Get fill color.
     * @note added in 2.1 */
    QColor fillColor() const override { return color(); }
    /** Set fill color.
     * @note added in 2.1 */
    void setFillColor( const QColor& color ) override { setColor( color ); }

    double outlineWidth() const { return mOutlineWidth; }
    void setOutlineWidth( double w ) { mOutlineWidth = w; }

    void setOutlineWidthUnit( QgsSymbolV2::OutputUnit u ) { mOutlineWidthUnit = u; }
    QgsSymbolV2::OutputUnit outlineWidthUnit() const { return mOutlineWidthUnit; }

    void setOutlineWidthMapUnitScale( const QgsMapUnitScale& scale ) { mOutlineWidthMapUnitScale = scale; }
    const QgsMapUnitScale& outlineWidthMapUnitScale() const { return mOutlineWidthMapUnitScale; }

    bool writeDxf( QgsDxfExport &e, double mmMapUnitScaleFactor, const QString &layerName, QgsSymbolV2RenderContext &context, QPointF shift = QPointF( 0.0, 0.0 ) ) const override;

    void setOutputUnit( QgsSymbolV2::OutputUnit unit ) override;
    QgsSymbolV2::OutputUnit outputUnit() const override;

    void setMapUnitScale( const QgsMapUnitScale& scale ) override;
    QgsMapUnitScale mapUnitScale() const override;

    QRectF bounds( QPointF point, QgsSymbolV2RenderContext& context ) override;

  protected:
    void drawMarker( QPainter* p, QgsSymbolV2RenderContext& context );

    bool prepareShape( const QString& name = QString() );
    bool prepareShape( const QString& name, QPolygonF &polygon ) const;
    bool preparePath( QString name = QString() );

    /** Prepares cache image
    @return true in case of success, false if cache image size too large*/
    bool prepareCache( QgsSymbolV2RenderContext& context );

    QColor mBorderColor;
    Qt::PenStyle mOutlineStyle;
    double mOutlineWidth;
    QgsSymbolV2::OutputUnit mOutlineWidthUnit;
    QgsMapUnitScale mOutlineWidthMapUnitScale;
    Qt::PenJoinStyle mPenJoinStyle;
    QPen mPen;
    QBrush mBrush;
    QPolygonF mPolygon;
    QPainterPath mPath;
    QString mName;
    QImage mCache;
    QPen mSelPen;
    QBrush mSelBrush;
    QImage mSelCache;
    bool mUsingCache;

    //Maximum width/height of cache image
    static const int mMaximumCacheWidth = 3000;

  private:

    double calculateSize( QgsSymbolV2RenderContext& context, bool& hasDataDefinedSize ) const;
    void calculateOffsetAndRotation( QgsSymbolV2RenderContext& context, double scaledSize, bool& hasDataDefinedRotation, QPointF& offset, double& angle ) const;
    bool symbolNeedsBrush( const QString& symbolName ) const;
};

//////////

#define DEFAULT_SVGMARKER_NAME         "/crosses/Star1.svg"
#define DEFAULT_SVGMARKER_SIZE         2*DEFAULT_POINT_SIZE
#define DEFAULT_SVGMARKER_ANGLE        0

class CORE_EXPORT QgsSvgMarkerSymbolLayerV2 : public QgsMarkerSymbolLayerV2
{
  public:
    QgsSvgMarkerSymbolLayerV2( const QString& name = DEFAULT_SVGMARKER_NAME,
                               double size = DEFAULT_SVGMARKER_SIZE,
                               double angle = DEFAULT_SVGMARKER_ANGLE,
                               QgsSymbolV2::ScaleMethod scaleMethod = DEFAULT_SCALE_METHOD );

    // static stuff

    static QgsSymbolLayerV2* create( const QgsStringMap& properties = QgsStringMap() );
    static QgsSymbolLayerV2* createFromSld( QDomElement &element );

    // implemented from base classes

    QString layerType() const override;

    void startRender( QgsSymbolV2RenderContext& context ) override;

    void stopRender( QgsSymbolV2RenderContext& context ) override;

    void renderPoint( QPointF point, QgsSymbolV2RenderContext& context ) override;

    QgsStringMap properties() const override;

    QgsSvgMarkerSymbolLayerV2* clone() const override;

    void writeSldMarker( QDomDocument &doc, QDomElement &element, const QgsStringMap& props ) const override;

    QString path() const { return mPath; }
    void setPath( const QString& path );

    QColor fillColor() const override { return color(); }
    void setFillColor( const QColor& color ) override { setColor( color ); }

    QColor outlineColor() const override { return mOutlineColor; }
    void setOutlineColor( const QColor& c ) override { mOutlineColor = c; }

    double outlineWidth() const { return mOutlineWidth; }
    void setOutlineWidth( double w ) { mOutlineWidth = w; }

    void setOutlineWidthUnit( QgsSymbolV2::OutputUnit unit ) { mOutlineWidthUnit = unit; }
    QgsSymbolV2::OutputUnit outlineWidthUnit() const { return mOutlineWidthUnit; }

    void setOutlineWidthMapUnitScale( const QgsMapUnitScale& scale ) { mOutlineWidthMapUnitScale = scale; }
    const QgsMapUnitScale& outlineWidthMapUnitScale() const { return mOutlineWidthMapUnitScale; }

    void setOutputUnit( QgsSymbolV2::OutputUnit unit ) override;
    QgsSymbolV2::OutputUnit outputUnit() const override;

    void setMapUnitScale( const QgsMapUnitScale& scale ) override;
    QgsMapUnitScale mapUnitScale() const override;

    bool writeDxf( QgsDxfExport& e, double mmMapUnitScaleFactor, const QString& layerName, QgsSymbolV2RenderContext &context, QPointF shift = QPointF( 0.0, 0.0 ) ) const override;

    QRectF bounds( QPointF point, QgsSymbolV2RenderContext& context ) override;

  protected:
    QString mPath;

    //param(fill), param(outline), param(outline-width) are going
    //to be replaced in memory
    QColor mOutlineColor;
    double mOutlineWidth;
    QgsSymbolV2::OutputUnit mOutlineWidthUnit;
    QgsMapUnitScale mOutlineWidthMapUnitScale;

  private:
    double calculateSize( QgsSymbolV2RenderContext& context, bool& hasDataDefinedSize ) const;
    void calculateOffsetAndRotation( QgsSymbolV2RenderContext& context, double scaledSize, QPointF& offset, double& angle ) const;

};


//////////

#define POINT2MM(x) ( (x) * 25.4 / 72 ) // point is 1/72 of inch
#define MM2POINT(x) ( (x) * 72 / 25.4 )

#define DEFAULT_FONTMARKER_FONT   "Dingbats"
#define DEFAULT_FONTMARKER_CHR    QChar('A')
#define DEFAULT_FONTMARKER_SIZE   POINT2MM(12)
#define DEFAULT_FONTMARKER_COLOR  QColor(Qt::black)
#define DEFAULT_FONTMARKER_BORDERCOLOR  QColor(Qt::white)
#define DEFAULT_FONTMARKER_JOINSTYLE    Qt::MiterJoin
#define DEFAULT_FONTMARKER_ANGLE  0

class CORE_EXPORT QgsFontMarkerSymbolLayerV2 : public QgsMarkerSymbolLayerV2
{
  public:
    QgsFontMarkerSymbolLayerV2( const QString& fontFamily = DEFAULT_FONTMARKER_FONT,
                                QChar chr = DEFAULT_FONTMARKER_CHR,
                                double pointSize = DEFAULT_FONTMARKER_SIZE,
                                const QColor& color = DEFAULT_FONTMARKER_COLOR,
                                double angle = DEFAULT_FONTMARKER_ANGLE );

    ~QgsFontMarkerSymbolLayerV2();

    // static stuff

    static QgsSymbolLayerV2* create( const QgsStringMap& properties = QgsStringMap() );
    static QgsSymbolLayerV2* createFromSld( QDomElement &element );

    // implemented from base classes

    QString layerType() const override;

    void startRender( QgsSymbolV2RenderContext& context ) override;

    void stopRender( QgsSymbolV2RenderContext& context ) override;

    void renderPoint( QPointF point, QgsSymbolV2RenderContext& context ) override;

    QgsStringMap properties() const override;

    QgsFontMarkerSymbolLayerV2* clone() const override;

    void writeSldMarker( QDomDocument &doc, QDomElement &element, const QgsStringMap& props ) const override;

    // new methods

    QString fontFamily() const { return mFontFamily; }
    void setFontFamily( const QString& family ) { mFontFamily = family; }

    QChar character() const { return mChr; }
    void setCharacter( QChar ch ) { mChr = ch; }

    /** Get outline color.
     * @note added in 2.16 */
    QColor outlineColor() const override { return mOutlineColor; }
    /** Set outline color.
     * @note added in 2.16 */
    void setOutlineColor( const QColor& color ) override { mOutlineColor = color; }

    /** Get outline width.
     * @note added in 2.16 */
    double outlineWidth() const { return mOutlineWidth; }
    /** Set outline width.
     * @note added in 2.16 */
    void setOutlineWidth( double width ) { mOutlineWidth = width; }

    /** Get outline width unit.
     * @note added in 2.16 */
    QgsSymbolV2::OutputUnit outlineWidthUnit() const { return mOutlineWidthUnit; }
    /** Set outline width unit.
     * @note added in 2.16 */
    void setOutlineWidthUnit( QgsSymbolV2::OutputUnit unit ) { mOutlineWidthUnit = unit; }

    /** Get outline width map unit scale.
     * @note added in 2.16 */
    const QgsMapUnitScale& outlineWidthMapUnitScale() const { return mOutlineWidthMapUnitScale; }
    /** Set outline width map unit scale.
     * @note added in 2.16 */
    void setOutlineWidthMapUnitScale( const QgsMapUnitScale& scale ) { mOutlineWidthMapUnitScale = scale; }

    /** Get outline join style.
     * @note added in 2.16 */
    Qt::PenJoinStyle penJoinStyle() const { return mPenJoinStyle; }
    /** Set outline join style.
     * @note added in 2.16 */
    void setPenJoinStyle( Qt::PenJoinStyle style ) { mPenJoinStyle = style; }

    QRectF bounds( QPointF point, QgsSymbolV2RenderContext& context ) override;

  protected:

    QString mFontFamily;
    QFontMetrics* mFontMetrics;
    QChar mChr;

    double mChrWidth;
    QPointF mChrOffset;
    QFont mFont;
    double mOrigSize;

  private:

    QColor mOutlineColor;
    double mOutlineWidth;
    QgsSymbolV2::OutputUnit mOutlineWidthUnit;
    QgsMapUnitScale mOutlineWidthMapUnitScale;
    Qt::PenJoinStyle mPenJoinStyle;

    QPen mPen;
    QBrush mBrush;

    QString characterToRender( QgsSymbolV2RenderContext& context, QPointF& charOffset, double& charWidth );
    void calculateOffsetAndRotation( QgsSymbolV2RenderContext& context, double scaledSize, bool& hasDataDefinedRotation, QPointF& offset, double& angle ) const;
    double calculateSize( QgsSymbolV2RenderContext& context );
};


#endif


