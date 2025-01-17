/***************************************************************************
                              qgswfssourceselect.cpp
                              -------------------
  begin                : August 25, 2006
  copyright            : (C) 2016 by Marco Hugentobler
  email                : marco dot hugentobler at karto dot baug dot ethz dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgswfsconstants.h"
#include "qgswfssourceselect.h"
#include "qgswfsconnection.h"
#include "qgswfscapabilities.h"
#include "qgswfsprovider.h"
#include "qgswfsdatasourceuri.h"
#include "qgsnewhttpconnection.h"
#include "qgsgenericprojectionselector.h"
#include "qgsexpressionbuilderdialog.h"
#include "qgscontexthelp.h"
#include "qgsproject.h"
#include "qgscoordinatereferencesystem.h"
#include "qgscoordinatetransform.h"
#include "qgslogger.h"
#include "qgsmanageconnectionsdialog.h"

#include <QDomDocument>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QSettings>
#include <QFileDialog>
#include <QPainter>

enum
{
  MODEL_IDX_TITLE,
  MODEL_IDX_NAME,
  MODEL_IDX_ABSTRACT,
  MODEL_IDX_FILTER
};

QgsWFSSourceSelect::QgsWFSSourceSelect( QWidget* parent, Qt::WindowFlags fl, bool embeddedMode )
    : QDialog( parent, fl )
    , mCapabilities( nullptr )
{
  setupUi( this );

  if ( embeddedMode )
  {
    buttonBox->button( QDialogButtonBox::Close )->hide();
  }

  mAddButton = new QPushButton( tr( "&Add" ) );
  mAddButton->setEnabled( false );

  mBuildQueryButton = new QPushButton( tr( "&Build query" ) );
  mBuildQueryButton->setToolTip( tr( "Build query" ) );
  mBuildQueryButton->setDisabled( true );


  buttonBox->addButton( mAddButton, QDialogButtonBox::ActionRole );
  connect( mAddButton, SIGNAL( clicked() ), this, SLOT( addLayer() ) );

  buttonBox->addButton( mBuildQueryButton, QDialogButtonBox::ActionRole );
  connect( mBuildQueryButton, SIGNAL( clicked() ), this, SLOT( buildQueryButtonClicked() ) );

  connect( buttonBox, SIGNAL( rejected() ), this, SLOT( reject() ) );
  connect( btnNew, SIGNAL( clicked() ), this, SLOT( addEntryToServerList() ) );
  connect( btnEdit, SIGNAL( clicked() ), this, SLOT( modifyEntryOfServerList() ) );
  connect( btnDelete, SIGNAL( clicked() ), this, SLOT( deleteEntryOfServerList() ) );
  connect( btnConnect, SIGNAL( clicked() ), this, SLOT( connectToServer() ) );
  connect( btnChangeSpatialRefSys, SIGNAL( clicked() ), this, SLOT( changeCRS() ) );
  connect( lineFilter, SIGNAL( textChanged( QString ) ), this, SLOT( filterChanged( QString ) ) );
  populateConnectionList();
  mProjectionSelector = new QgsGenericProjectionSelector( this );
  mProjectionSelector->setMessage();

  mItemDelegate = new QgsWFSItemDelegate( treeView );
  treeView->setItemDelegate( mItemDelegate );

  QSettings settings;
  QgsDebugMsg( "restoring settings" );
  restoreGeometry( settings.value( "/Windows/WFSSourceSelect/geometry" ).toByteArray() );
  cbxUseTitleLayerName->setChecked( settings.value( "/Windows/WFSSourceSelect/UseTitleLayerName", false ).toBool() );
  mHoldDialogOpen->setChecked( settings.value( "/Windows/WFSSourceSelect/HoldDialogOpen", false ).toBool() );

  mModel = new QStandardItemModel();
  mModel->setHorizontalHeaderItem( MODEL_IDX_TITLE, new QStandardItem( "Title" ) );
  mModel->setHorizontalHeaderItem( MODEL_IDX_NAME, new QStandardItem( "Name" ) );
  mModel->setHorizontalHeaderItem( MODEL_IDX_ABSTRACT, new QStandardItem( "Abstract" ) );
  mModel->setHorizontalHeaderItem( MODEL_IDX_FILTER, new QStandardItem( "Filter" ) );

  mModelProxy = new QSortFilterProxyModel( this );
  mModelProxy->setSourceModel( mModel );
  mModelProxy->setSortCaseSensitivity( Qt::CaseInsensitive );
  treeView->setModel( mModelProxy );

  connect( treeView, SIGNAL( doubleClicked( const QModelIndex& ) ), this, SLOT( treeWidgetItemDoubleClicked( const QModelIndex& ) ) );
  connect( treeView->selectionModel(), SIGNAL( currentRowChanged( QModelIndex, QModelIndex ) ), this, SLOT( treeWidgetCurrentRowChanged( const QModelIndex&, const QModelIndex& ) ) );
}

QgsWFSSourceSelect::~QgsWFSSourceSelect()
{
  QSettings settings;
  QgsDebugMsg( "saving settings" );
  settings.setValue( "/Windows/WFSSourceSelect/geometry", saveGeometry() );
  settings.setValue( "/Windows/WFSSourceSelect/UseTitleLayerName", cbxUseTitleLayerName->isChecked() );
  settings.setValue( "/Windows/WFSSourceSelect/HoldDialogOpen", mHoldDialogOpen->isChecked() );

  delete mItemDelegate;
  delete mProjectionSelector;
  delete mCapabilities;
  delete mModel;
  delete mModelProxy;
  delete mAddButton;
  delete mBuildQueryButton;
}

void QgsWFSSourceSelect::populateConnectionList()
{
  QStringList keys = QgsWFSConnection::connectionList();

  QStringList::Iterator it = keys.begin();
  cmbConnections->clear();
  while ( it != keys.end() )
  {
    cmbConnections->addItem( *it );
    ++it;
  }

  if ( keys.begin() != keys.end() )
  {
    // Connections available - enable various buttons
    btnConnect->setEnabled( true );
    btnEdit->setEnabled( true );
    btnDelete->setEnabled( true );
    btnSave->setEnabled( true );
  }
  else
  {
    // No connections available - disable various buttons
    btnConnect->setEnabled( false );
    btnEdit->setEnabled( false );
    btnDelete->setEnabled( false );
    btnSave->setEnabled( false );
  }

  //set last used connection
  QString selectedConnection = QgsWFSConnection::selectedConnection();
  int index = cmbConnections->findText( selectedConnection );
  if ( index != -1 )
  {
    cmbConnections->setCurrentIndex( index );
  }

  QgsWFSConnection connection( cmbConnections->currentText() );
  delete mCapabilities;
  mCapabilities = new QgsWFSCapabilities( connection.uri().uri() );
  connect( mCapabilities, SIGNAL( gotCapabilities() ), this, SLOT( capabilitiesReplyFinished() ) );
}

QString QgsWFSSourceSelect::getPreferredCrs( const QSet<QString>& crsSet ) const
{
  if ( crsSet.size() < 1 )
  {
    return "";
  }

  //first: project CRS
  long ProjectCRSID = QgsProject::instance()->readNumEntry( "SpatialRefSys", "/ProjectCRSID", -1 );
  //convert to EPSG
  QgsCoordinateReferenceSystem projectRefSys( ProjectCRSID, QgsCoordinateReferenceSystem::InternalCrsId );
  QString ProjectCRS;
  if ( projectRefSys.isValid() )
  {
    ProjectCRS = projectRefSys.authid();
  }

  if ( !ProjectCRS.isEmpty() && crsSet.contains( ProjectCRS ) )
  {
    return ProjectCRS;
  }

  //second: WGS84
  if ( crsSet.contains( GEO_EPSG_CRS_AUTHID ) )
  {
    return GEO_EPSG_CRS_AUTHID;
  }

  //third: first entry in set
  return *( crsSet.constBegin() );
}

void QgsWFSSourceSelect::capabilitiesReplyFinished()
{
  btnConnect->setEnabled( true );

  if ( !mCapabilities )
    return;
  QgsWFSCapabilities::ErrorCode err = mCapabilities->errorCode();
  if ( err != QgsWFSCapabilities::NoError )
  {
    QString title;
    switch ( err )
    {
      case QgsWFSCapabilities::NetworkError:
        title = tr( "Network Error" );
        break;
      case QgsWFSCapabilities::XmlError:
        title = tr( "Capabilities document is not valid" );
        break;
      case QgsWFSCapabilities::ServerExceptionError:
        title = tr( "Server Exception" );
        break;
      default:
        tr( "Error" );
        break;
    }
    // handle errors
    QMessageBox::critical( nullptr, title, mCapabilities->errorMessage() );

    mAddButton->setEnabled( false );
    return;
  }

  QgsWFSCapabilities::Capabilities caps = mCapabilities->capabilities();

  mAvailableCRS.clear();
  Q_FOREACH ( const QgsWFSCapabilities::FeatureType& featureType, caps.featureTypes )
  {
    // insert the typenames, titles and abstracts into the tree view
    QStandardItem* titleItem = new QStandardItem( featureType.title );
    QStandardItem* nameItem = new QStandardItem( featureType.name );
    QStandardItem* abstractItem = new QStandardItem( featureType.abstract );
    abstractItem->setToolTip( "<font color=black>" + featureType.abstract  + "</font>" );
    abstractItem->setTextAlignment( Qt::AlignLeft | Qt::AlignTop );
    QStandardItem* filterItem = new QStandardItem();

    typedef QList< QStandardItem* > StandardItemList;
    mModel->appendRow( StandardItemList() << titleItem << nameItem << abstractItem << filterItem );

    // insert the available CRS into mAvailableCRS
    mAvailableCRS.insert( featureType.name, featureType.crslist );
  }

  if ( !caps.featureTypes.isEmpty() )
  {
    treeView->resizeColumnToContents( MODEL_IDX_TITLE );
    treeView->resizeColumnToContents( MODEL_IDX_NAME );
    treeView->resizeColumnToContents( MODEL_IDX_ABSTRACT );
    for ( int i = MODEL_IDX_TITLE; i < MODEL_IDX_ABSTRACT; i++ )
    {
      if ( treeView->columnWidth( i ) > 300 )
      {
        treeView->setColumnWidth( i, 300 );
      }
    }
    if ( treeView->columnWidth( MODEL_IDX_ABSTRACT ) > 150 )
    {
      treeView->setColumnWidth( MODEL_IDX_ABSTRACT, 150 );
    }
    btnChangeSpatialRefSys->setEnabled( true );
    treeView->selectionModel()->select( mModel->index( 0, 0 ), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows );
    treeView->setFocus();
  }
  else
  {
    QMessageBox::information( nullptr, tr( "No Layers" ), tr( "capabilities document contained no layers." ) );
    mAddButton->setEnabled( false );
    mBuildQueryButton->setEnabled( false );
  }
}

void QgsWFSSourceSelect::addEntryToServerList()
{
  QgsNewHttpConnection nc( nullptr, QgsWFSConstants::CONNECTIONS_WFS );
  nc.setWindowTitle( tr( "Create a new WFS connection" ) );

  if ( nc.exec() )
  {
    populateConnectionList();
    emit connectionsChanged();
  }
}

void QgsWFSSourceSelect::modifyEntryOfServerList()
{
  QgsNewHttpConnection nc( nullptr, QgsWFSConstants::CONNECTIONS_WFS, cmbConnections->currentText() );
  nc.setWindowTitle( tr( "Modify WFS connection" ) );

  if ( nc.exec() )
  {
    populateConnectionList();
    emit connectionsChanged();
  }
}

void QgsWFSSourceSelect::deleteEntryOfServerList()
{
  QString msg = tr( "Are you sure you want to remove the %1 connection and all associated settings?" )
                .arg( cmbConnections->currentText() );
  QMessageBox::StandardButton result = QMessageBox::information( this, tr( "Confirm Delete" ), msg, QMessageBox::Ok | QMessageBox::Cancel );
  if ( result == QMessageBox::Ok )
  {
    QgsWFSConnection::deleteConnection( cmbConnections->currentText() );
    cmbConnections->removeItem( cmbConnections->currentIndex() );
    emit connectionsChanged();

    if ( cmbConnections->count() > 0 )
    {
      // Connections available - enable various buttons
      btnConnect->setEnabled( true );
      btnEdit->setEnabled( true );
      btnDelete->setEnabled( true );
      btnSave->setEnabled( true );
    }
    else
    {
      // No connections available - disable various buttons
      btnConnect->setEnabled( false );
      btnEdit->setEnabled( false );
      btnDelete->setEnabled( false );
      btnSave->setEnabled( false );
    }
  }
}

void QgsWFSSourceSelect::connectToServer()
{
  btnConnect->setEnabled( false );
  if ( mModel )
  {
    mModel->removeRows( 0, mModel->rowCount() );
  }
  if ( mCapabilities )
  {
    mCapabilities->requestCapabilities( false );
  }
}


void QgsWFSSourceSelect::addLayer()
{
  //get selected entry in treeview
  QModelIndex currentIndex = treeView->selectionModel()->currentIndex();
  if ( !currentIndex.isValid() )
  {
    return;
  }

  QgsWFSConnection connection( cmbConnections->currentText() );

  QString pCrsString( labelCoordRefSys->text() );

  //create layers that user selected from this WFS source
  QModelIndexList list = treeView->selectionModel()->selectedRows();
  for ( int i = 0; i < list.size(); i++ )
  { //add a wfs layer to the map
    QModelIndex idx = mModelProxy->mapToSource( list[i] );
    if ( !idx.isValid() )
    {
      continue;
    }
    int row = idx.row();
    QString typeName = mModel->item( row, MODEL_IDX_NAME )->text(); //WFS repository's name for layer
    QString titleName = mModel->item( row, MODEL_IDX_TITLE )->text(); //WFS type name title for layer name (if option is set)
    QString filter = mModel->item( row, MODEL_IDX_FILTER )->text(); //optional filter specified by user
    QString layerName = typeName;
    if ( cbxUseTitleLayerName->isChecked() && !titleName.isEmpty() )
    {
      layerName = titleName;
    }
    QgsDebugMsg( "Layer " + typeName + " Filter is " + filter );

    //is "Only request features overlapping the view extent" checked?
    if ( !cbxFeatureCurrentViewExtent->isChecked() )
    { //no: entire WFS layer will be retrieved and cached
      mUri = QgsWFSDataSourceURI::build( connection.uri().uri(), typeName, pCrsString, filter );
    }
    else
    { //yes
      mUri = QgsWFSDataSourceURI::build( connection.uri().uri(), typeName, pCrsString, filter, true );
    }
    emit addWfsLayer( mUri, layerName );
  }

  if ( !mHoldDialogOpen->isChecked() )
  {
    accept();
  }
}

void QgsWFSSourceSelect::buildQuery( const QModelIndex& index )
{
  if ( !index.isValid() )
  {
    return;
  }
  QModelIndex filterIndex = index.sibling( index.row(), MODEL_IDX_FILTER );
  QString typeName = index.sibling( index.row(), MODEL_IDX_NAME ).data().toString();

  //get available fields for wfs layer
  QgsWFSConnection connection( cmbConnections->currentText() );
  QgsWFSDataSourceURI uri( connection.uri().uri() );
  uri.setTypeName( typeName );
  QgsWFSProvider p( uri.uri() );
  if ( !p.isValid() )
  {
    return;
  }

  QgsFields fields( p.fields() );

  //show expression builder
  QgsExpressionBuilderDialog d( nullptr, filterIndex.data().toString() );

  //add available attributes to expression builder
  QgsExpressionBuilderWidget* w = d.expressionBuilder();
  if ( !w )
  {
    return;
  }

  w->loadFieldNames( fields );

  if ( d.exec() == QDialog::Accepted )
  {
    QgsDebugMsg( "Expression text = " + w->expressionText() );
    mModelProxy->setData( filterIndex, QVariant( w->expressionText() ) );
  }
}

void QgsWFSSourceSelect::changeCRS()
{
  if ( mProjectionSelector->exec() )
  {
    QString crsString = mProjectionSelector->selectedAuthId();
    labelCoordRefSys->setText( crsString );
  }
}

void QgsWFSSourceSelect::changeCRSFilter()
{
  QgsDebugMsg( "changeCRSFilter called" );
  //evaluate currently selected typename and set the CRS filter in mProjectionSelector
  QModelIndex currentIndex = treeView->selectionModel()->currentIndex();
  if ( currentIndex.isValid() )
  {
    QString currentTypename = currentIndex.sibling( currentIndex.row(), MODEL_IDX_NAME ).data().toString();
    QgsDebugMsg( QString( "the current typename is: %1" ).arg( currentTypename ) );

    QMap<QString, QStringList >::const_iterator crsIterator = mAvailableCRS.find( currentTypename );
    if ( crsIterator != mAvailableCRS.end() )
    {
      QSet<QString> crsNames( crsIterator->toSet() );

      if ( mProjectionSelector )
      {
        mProjectionSelector->setOgcWmsCrsFilter( crsNames );
        QString preferredCRS = getPreferredCrs( crsNames ); //get preferred EPSG system
        if ( !preferredCRS.isEmpty() )
        {
          QgsCoordinateReferenceSystem refSys;
          refSys.createFromOgcWmsCrs( preferredCRS );
          mProjectionSelector->setSelectedCrsId( refSys.srsid() );

          labelCoordRefSys->setText( preferredCRS );
        }
      }
    }
  }
}

void QgsWFSSourceSelect::on_cmbConnections_activated( int index )
{
  Q_UNUSED( index );
  QgsWFSConnection::setSelectedConnection( cmbConnections->currentText() );

  QgsWFSConnection connection( cmbConnections->currentText() );

  delete mCapabilities;
  mCapabilities = new QgsWFSCapabilities( connection.uri().uri() );
  connect( mCapabilities, SIGNAL( gotCapabilities() ), this, SLOT( capabilitiesReplyFinished() ) );
}

void QgsWFSSourceSelect::on_btnSave_clicked()
{
  QgsManageConnectionsDialog dlg( this, QgsManageConnectionsDialog::Export, QgsManageConnectionsDialog::WFS );
  dlg.exec();
}

void QgsWFSSourceSelect::on_btnLoad_clicked()
{
  QString fileName = QFileDialog::getOpenFileName( this, tr( "Load connections" ), QDir::homePath(),
                     tr( "XML files (*.xml *XML)" ) );
  if ( fileName.isEmpty() )
  {
    return;
  }

  QgsManageConnectionsDialog dlg( this, QgsManageConnectionsDialog::Import, QgsManageConnectionsDialog::WFS, fileName );
  dlg.exec();
  populateConnectionList();
  emit connectionsChanged();
}

void QgsWFSSourceSelect::treeWidgetItemDoubleClicked( const QModelIndex& index )
{
  QgsDebugMsg( "double click called" );
  buildQuery( index );
}

void QgsWFSSourceSelect::treeWidgetCurrentRowChanged( const QModelIndex & current, const QModelIndex & previous )
{
  Q_UNUSED( previous )
  QgsDebugMsg( "treeWidget_currentRowChanged called" );
  changeCRSFilter();
  mBuildQueryButton->setEnabled( current.isValid() );
  mAddButton->setEnabled( current.isValid() );
}

void QgsWFSSourceSelect::buildQueryButtonClicked()
{
  QgsDebugMsg( "mBuildQueryButton click called" );
  buildQuery( treeView->selectionModel()->currentIndex() );
}

void QgsWFSSourceSelect::filterChanged( const QString& text )
{
  QgsDebugMsg( "WFS FeatureType filter changed to :" + text );
  QRegExp::PatternSyntax mySyntax = QRegExp::PatternSyntax( QRegExp::RegExp );
  Qt::CaseSensitivity myCaseSensitivity = Qt::CaseInsensitive;
  QRegExp myRegExp( text, myCaseSensitivity, mySyntax );
  mModelProxy->setFilterRegExp( myRegExp );
  mModelProxy->sort( mModelProxy->sortColumn(), mModelProxy->sortOrder() );
}

QSize QgsWFSItemDelegate::sizeHint( const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
  QVariant indexData;
  indexData = index.data( Qt::DisplayRole );
  if ( indexData.isNull() )
  {
    return QSize();
  }
  QString data = indexData.toString();
  QSize size = option.fontMetrics.boundingRect( data ).size();
  size.setHeight( size.height() + 2 );
  return size;
}
