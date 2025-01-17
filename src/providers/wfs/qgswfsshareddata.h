/***************************************************************************
    qgswfsshareddata.h
    ---------------------
    begin                : March 2016
    copyright            : (C) 2016 by Even Rouault
    email                : even.rouault at spatialys.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSWFSSHAREDDATA_H
#define QGSWFSSHAREDDATA_H

#include "qgsspatialindex.h"
#include "qgswfsfeatureiterator.h"
#include "qgswfsrequest.h"

/** This class holds data, and logic, shared between QgsWFSProvider, QgsWFSFeatureIterator
 *  and QgsWFSFeatureDownloader. It manages the on-disk cache, as a Spatialite
 *  database.
 *
 *  The structure of the table in the database is the following one :
 *  - attribute fields of the DescribeFeatureType response
 *  - __qgis_gen_counter: generation counter
 *  - __qgis_gmlid: feature 'fid' or 'gml:id'
 *  - __qgis_hexwkb_geom: feature geometry as a hexadecimal encoded WKB string.
 *  - geometry: polygon with the bounding box of the geometry.
 *
 *  The generation counter is a synchronization mechanism between the iterator
 *  that will try to return cached features first and then downloaded features.
 *  It avoids the iterator to return features in duplicates, by returning features
 *  that have just been serialized by the live downloader and notified to the
 *  iterator.
 *
 *  The reason for not storing directly the geometry is that we may potentially
 *  store in the future non-linear geometries that aren't handled by Spatialite.
 *
 *  It contains also methods used in WFS-T context to update the cache content,
 *  from the changes initiated by the user.
 */
class QgsWFSSharedData : public QObject
{
    Q_OBJECT
  public:
    explicit QgsWFSSharedData( const QString& uri );
    ~QgsWFSSharedData();

    /** Used by a QgsWFSFeatureIterator to start a downloader and get the
        generation counter. */
    int registerToCache( QgsWFSFeatureIterator* iterator, QgsRectangle rect = QgsRectangle() );

    /** Used by the rewind() method of an iterator so as to get the up-to-date
        generation counter. */
    int getUpdatedCounter();

    /** Used by the background downloader to serialize downloaded features into
        the cache. Also used by a WFS-T insert operation */
    void serializeFeatures( QVector<QgsWFSFeatureGmlIdPair>& featureList );

    /** Called by QgsWFSFeatureDownloader::run() at the end of the download process. */
    void endOfDownload( bool success, int featureCount );

    /** Used by QgsWFSProvider::reloadData(). The effect is to invalid
        all the caching state, so that a new request results in fresh download */
    void invalidateCache();

    /** Give a feature id, find the correspond fid/gml.id. Used by WFS-T */
    QString findGmlId( QgsFeatureId fid );

    /** Delete from the on-disk cache the features of given fid. Used by WFS-T */
    bool deleteFeatures( const QgsFeatureIds& fidlist );

    /** Change into the on-disk cache the passed geometries. Used by WFS-T */
    bool changeGeometryValues( const QgsGeometryMap &geometry_map );

    /** Change into the on-disk cache the passed attributes. Used by WFS-T */
    bool changeAttributeValues( const QgsChangedAttributesMap &attr_map );

    /** Force an update of the feature count */
    void setFeatureCount( int featureCount );

    /** Return layer feature count. Might issue a GetFeature resultType=hits request */
    int getFeatureCount( bool issueRequestIfNeeded = true );

    /** Return whether the feature count is exact, or approximate/transient */
    bool isFeatureCountExact() const { return mFeatureCountExact; }

    /** Return whether the server support RESULTTYPE=hits */
    bool supportsHits() const { return mSupportsHits; }

    /** Compute WFS filter from the filter in the URI */
    void computeFilter();

    /** Return WFS filter */
    QString WFSFilter() const { return mWFSFilter; }

    /** Return srsName */
    QString srsName() const;

  protected:
    friend class QgsWFSFeatureIterator;
    friend class QgsWFSFeatureDownloader;
    friend class QgsWFSProvider;

    /** Datasource URI */
    QgsWFSDataSourceURI mURI;

    /** WFS version to use. Comes from GetCapabilities response */
    QString mWFSVersion;

    /** Source CRS*/
    QgsCoordinateReferenceSystem mSourceCRS;

    /** Attribute fields of the layer */
    QgsFields mFields;

    /** Name of geometry attribute */
    QString mGeometryAttribute;

    /** The data provider of the on-disk cache */
    QgsVectorDataProvider* mCacheDataProvider;

    /** Current BBOX used by the downloader */
    QgsRectangle mRect;

    /** Server-side or user-side limit of downloaded features (in a single GetFeature()). Valid if > 0 */
    int mMaxFeatures;

    /** Server-side limit of downloaded features. Valid if > 0 */
    int mMaxFeaturesServer;

    /** Whether resultType=hits is supported */
    bool mSupportsHits;

    /** Whether paging is enabled (WFS 2.0) */
    bool mSupportsPaging;

  private:

    /** Main mutex to protect most data members that can be modified concurrently */
    QMutex mMutex;

    /** Mutex used specifically by registerToCache() */
    QMutex mMutexRegisterToCache;

    /** Mutex used only by serializeFeatures() */
    QMutex mCacheWriteMutex;

    /** WFS filter */
    QString mWFSFilter;

    /** The background feature downloader */
    QgsWFSThreadedFeatureDownloader* mDownloader;

    /** Whether the downloader has finished (or been cancelled) */
    bool mDownloadFinished;

    /** The generation counter. When a iterator is built or rewind, it gets the
        current value of the generation counter to query the features in the cache
        whose generation counter is <= the current value. That way the iterator
        can consume first cached features, and then deal with the features that are
        notified in live by the downloader. */
    int mGenCounter;

    /** Number of features of the layer */
    int mFeatureCount;

    /** Whether mFeatureCount value is exact or approximate / in construction */
    bool mFeatureCountExact;

    /** Filename of the on-disk cache */
    QString mCacheDbname;

    /** Tablename of the on-disk cache */
    QString mCacheTablename;

    /** Spatial index of requested cached regions */
    QgsSpatialIndex mCachedRegions;

    /** Requested cached regions */
    QVector< QgsFeature > mRegions;

    /** Whether a GetFeature hits request has been issued to retrieve the number of features */
    bool mGetFeatureHitsIssued;

    /** Number of features that have been cached, or attempted to be cached */
    int mTotalFeaturesAttemptedToBeCached;

    /** Returns the set of gmlIds that have already been downloaded and
        cached, so as to avoid to cache duplicates. */
    QSet<QString> getExistingCachedGmlIds( const QVector<QgsWFSFeatureGmlIdPair>& featureList );

    /** Create the on-disk cache and connect to it */
    bool createCache();
};

/** Utility class to issue a GetFeature resultType=hits request */
class QgsWFSFeatureHitsRequest: public QgsWFSRequest
{
    Q_OBJECT
  public:
    explicit QgsWFSFeatureHitsRequest( QgsWFSDataSourceURI& uri );
    ~QgsWFSFeatureHitsRequest();

    /** Return the feature count, or -1 in case of error */
    int getFeatureCount( const QString& WFSVersion, const QString& filter );

  protected:
    virtual QString errorMessageWithReason( const QString& reason ) override;
};

#endif // QGSWFSSHAREDDATA_H
