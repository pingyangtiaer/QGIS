/***************************************************************************
    qgsdualview.cpp
     --------------------------------------
    Date                 : 10.2.2013
    Copyright            : (C) 2013 Matthias Kuhn
    Email                : matthias at opengis dot ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsapplication.h"
#include "qgsactionmanager.h"
#include "qgsattributeform.h"
#include "qgsattributetablemodel.h"
#include "qgsdualview.h"
#include "qgsexpressionbuilderdialog.h"
#include "qgsfeaturelistmodel.h"
#include "qgsifeatureselectionmanager.h"
#include "qgsmapcanvas.h"
#include "qgsmaplayeractionregistry.h"
#include "qgsmessagelog.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorlayercache.h"

#include <QDialog>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>

QgsDualView::QgsDualView( QWidget* parent )
    : QStackedWidget( parent )
    , mEditorContext()
    , mMasterModel( nullptr )
    , mFilterModel( nullptr )
    , mFeatureListModel( nullptr )
    , mAttributeForm( nullptr )
    , mLayerCache( nullptr )
    , mProgressDlg( nullptr )
    , mFeatureSelectionManager( nullptr )
{
  setupUi( this );

  mConditionalFormatWidget->hide();

  mPreviewActionMapper = new QSignalMapper( this );

  mPreviewColumnsMenu = new QMenu( this );
  mActionPreviewColumnsMenu->setMenu( mPreviewColumnsMenu );

  // Set preview icon
  mActionExpressionPreview->setIcon( QgsApplication::getThemeIcon( "/mIconExpressionPreview.svg" ) );

  // Connect layer list preview signals
  connect( mActionExpressionPreview, SIGNAL( triggered() ), SLOT( previewExpressionBuilder() ) );
  connect( mPreviewActionMapper, SIGNAL( mapped( QObject* ) ), SLOT( previewColumnChanged( QObject* ) ) );
  connect( mFeatureList, SIGNAL( displayExpressionChanged( QString ) ), this, SLOT( previewExpressionChanged( QString ) ) );
}

void QgsDualView::init( QgsVectorLayer* layer, QgsMapCanvas* mapCanvas, const QgsFeatureRequest &request, const QgsAttributeEditorContext &context )
{
  mEditorContext = context;

  connect( mTableView, SIGNAL( willShowContextMenu( QMenu*, QModelIndex ) ), this, SLOT( viewWillShowContextMenu( QMenu*, QModelIndex ) ) );

  initLayerCache( layer, !request.filterRect().isNull() );
  initModels( mapCanvas, request );

  mConditionalFormatWidget->setLayer( layer );

  mTableView->setModel( mFilterModel );
  mFeatureList->setModel( mFeatureListModel );
  mAttributeForm = new QgsAttributeForm( layer, QgsFeature(), mEditorContext );
  if ( !mAttributeEditorScrollArea->layout() )
    mAttributeEditorScrollArea->setLayout( new QGridLayout() );
  mAttributeEditorScrollArea->layout()->addWidget( mAttributeForm );
  mAttributeEditorScrollArea->setWidget( mAttributeForm );

  mAttributeForm->hideButtonBox();

  connect( mAttributeForm, SIGNAL( attributeChanged( QString, QVariant ) ), this, SLOT( featureFormAttributeChanged() ) );
  connect( mMasterModel, SIGNAL( modelChanged() ), mAttributeForm, SLOT( refreshFeature() ) );

  if ( mFeatureListPreviewButton->defaultAction() )
    mFeatureList->setDisplayExpression( mDisplayExpression );
  else
    columnBoxInit();

  // This slows down load of the attribute table heaps and uses loads of memory.
  //mTableView->resizeColumnsToContents();

  mFeatureList->setEditSelection( QgsFeatureIds() << mFeatureListModel->idxToFid( mFeatureListModel->index( 0, 0 ) ) );
}

void QgsDualView::columnBoxInit()
{
  // load fields
  QList<QgsField> fields = mLayerCache->layer()->fields().toList();

  QString defaultField;

  // default expression: saved value
  QString displayExpression = mLayerCache->layer()->displayExpression();

  // if no display expression is saved: use display field instead
  if ( displayExpression.isEmpty() )
  {
    if ( !mLayerCache->layer()->displayField().isEmpty() )
    {
      defaultField = mLayerCache->layer()->displayField();
      displayExpression = QString( "COALESCE(\"%1\", '<NULL>')" ).arg( defaultField );
    }
  }

  // if neither display expression nor display field is saved...
  if ( displayExpression.isEmpty() )
  {
    QgsAttributeList pkAttrs = mLayerCache->layer()->pkAttributeList();

    if ( !pkAttrs.isEmpty() )
    {
      if ( pkAttrs.size() == 1 )
        defaultField = pkAttrs.at( 0 );

      // ... If there are primary key(s) defined
      QStringList pkFields;

      Q_FOREACH ( int attr, pkAttrs )
      {
        pkFields.append( "COALESCE(\"" + fields[attr].name() + "\", '<NULL>')" );
      }

      displayExpression = pkFields.join( "||', '||" );
    }
    else if ( !fields.isEmpty() )
    {
      if ( fields.size() == 1 )
        defaultField = fields.at( 0 ).name();

      // ... concat all fields
      QStringList fieldNames;
      Q_FOREACH ( const QgsField& field, fields )
      {
        fieldNames.append( "COALESCE(\"" + field.name() + "\", '<NULL>')" );
      }

      displayExpression = fieldNames.join( "||', '||" );
    }
    else
    {
      // ... there isn't really much to display
      displayExpression = "'[Please define preview text]'";
    }
  }

  mFeatureListPreviewButton->addAction( mActionExpressionPreview );
  mFeatureListPreviewButton->addAction( mActionPreviewColumnsMenu );

  Q_FOREACH ( const QgsField& field, fields )
  {
    int fieldIndex = mLayerCache->layer()->fieldNameIndex( field.name() );
    if ( fieldIndex == -1 )
      continue;

    if ( mLayerCache->layer()->editFormConfig()->widgetType( fieldIndex ) != "Hidden" )
    {
      QIcon icon = mLayerCache->layer()->fields().iconForField( fieldIndex );
      QString text = field.name();

      // Generate action for the preview popup button of the feature list
      QAction* previewAction = new QAction( icon, text, mFeatureListPreviewButton );
      mPreviewActionMapper->setMapping( previewAction, previewAction );
      connect( previewAction, SIGNAL( triggered() ), mPreviewActionMapper, SLOT( map() ) );
      mPreviewColumnsMenu->addAction( previewAction );

      if ( text == defaultField )
      {
        mFeatureListPreviewButton->setDefaultAction( previewAction );
      }
    }
  }

  // If there is no single field found as preview
  if ( !mFeatureListPreviewButton->defaultAction() )
  {
    mFeatureList->setDisplayExpression( displayExpression );
    mFeatureListPreviewButton->setDefaultAction( mActionExpressionPreview );
    mDisplayExpression = mFeatureList->displayExpression();
  }
  else
  {
    mFeatureListPreviewButton->defaultAction()->trigger();
  }
}

void QgsDualView::setView( QgsDualView::ViewMode view )
{
  setCurrentIndex( view );
}

QgsDualView::ViewMode QgsDualView::view() const
{
  return static_cast< QgsDualView::ViewMode >( currentIndex() );
}

void QgsDualView::setFilterMode( QgsAttributeTableFilterModel::FilterMode filterMode )
{
  mFilterModel->setFilterMode( filterMode );
  emit filterChanged();
}

void QgsDualView::setSelectedOnTop( bool selectedOnTop )
{
  mFilterModel->setSelectedOnTop( selectedOnTop );
}

void QgsDualView::initLayerCache( QgsVectorLayer* layer, bool cacheGeometry )
{
  // Initialize the cache
  QSettings settings;
  int cacheSize = settings.value( "/qgis/attributeTableRowCache", "10000" ).toInt();
  mLayerCache = new QgsVectorLayerCache( layer, cacheSize, this );
  mLayerCache->setCacheGeometry( cacheGeometry );
  if ( 0 == cacheSize || 0 == ( QgsVectorDataProvider::SelectAtId & mLayerCache->layer()->dataProvider()->capabilities() ) )
  {
    connect( mLayerCache, SIGNAL( progress( int, bool & ) ), this, SLOT( progress( int, bool & ) ) );
    connect( mLayerCache, SIGNAL( finished() ), this, SLOT( finished() ) );

    mLayerCache->setFullCache( true );
  }
}

void QgsDualView::initModels( QgsMapCanvas* mapCanvas, const QgsFeatureRequest& request )
{
  delete mFeatureListModel;
  delete mFilterModel;
  delete mMasterModel;

  mMasterModel = new QgsAttributeTableModel( mLayerCache, this );
  mMasterModel->setRequest( request );
  mMasterModel->setEditorContext( mEditorContext );

  connect( mMasterModel, SIGNAL( progress( int, bool & ) ), this, SLOT( progress( int, bool & ) ) );
  connect( mMasterModel, SIGNAL( finished() ), this, SLOT( finished() ) );

  connect( mConditionalFormatWidget, SIGNAL( rulesUpdated( QString ) ), mMasterModel, SLOT( fieldConditionalStyleChanged( QString ) ) );

  mMasterModel->loadLayer();

  mFilterModel = new QgsAttributeTableFilterModel( mapCanvas, mMasterModel, mMasterModel );

  connect( mFeatureList, SIGNAL( displayExpressionChanged( QString ) ), this, SIGNAL( displayExpressionChanged( QString ) ) );

  mFeatureListModel = new QgsFeatureListModel( mFilterModel, mFilterModel );
}

void QgsDualView::on_mFeatureList_aboutToChangeEditSelection( bool& ok )
{
  if ( mLayerCache->layer()->isEditable() && !mAttributeForm->save() )
    ok = false;
}

void QgsDualView::on_mFeatureList_currentEditSelectionChanged( const QgsFeature &feat )
{
  if ( !mLayerCache->layer()->isEditable() || mAttributeForm->save() )
  {
    mAttributeForm->setFeature( feat );
    setCurrentEditSelection( QgsFeatureIds() << feat.id() );
  }
  else
  {
    // Couldn't save feature
  }
}

void QgsDualView::setCurrentEditSelection( const QgsFeatureIds& fids )
{
  mFeatureList->setCurrentFeatureEdited( false );
  mFeatureList->setEditSelection( fids );
}

bool QgsDualView::saveEditChanges()
{
  return mAttributeForm->save();
}

void QgsDualView::openConditionalStyles()
{
  mConditionalFormatWidget->setVisible( !mConditionalFormatWidget->isVisible() );
  mConditionalFormatWidget->viewRules();
}

void QgsDualView::setMultiEditEnabled( bool enabled )
{
  if ( enabled )
    setView( AttributeEditor );

  mAttributeForm->setMode( enabled ? QgsAttributeForm::MultiEditMode : QgsAttributeForm::SingleEditMode );
}

void QgsDualView::previewExpressionBuilder()
{
  // Show expression builder
  QgsExpressionContext context;
  context << QgsExpressionContextUtils::globalScope()
  << QgsExpressionContextUtils::projectScope()
  << QgsExpressionContextUtils::layerScope( mLayerCache->layer() );

  QgsExpressionBuilderDialog dlg( mLayerCache->layer(), mFeatureList->displayExpression(), this, "generic", context );
  dlg.setWindowTitle( tr( "Expression based preview" ) );
  dlg.setExpressionText( mFeatureList->displayExpression() );

  if ( dlg.exec() == QDialog::Accepted )
  {
    mFeatureList->setDisplayExpression( dlg.expressionText() );
    mFeatureListPreviewButton->setDefaultAction( mActionExpressionPreview );
    mFeatureListPreviewButton->setPopupMode( QToolButton::MenuButtonPopup );
  }

  mDisplayExpression = mFeatureList->displayExpression();
}

void QgsDualView::previewColumnChanged( QObject* action )
{
  QAction* previewAction = qobject_cast< QAction* >( action );

  if ( previewAction )
  {
    if ( !mFeatureList->setDisplayExpression( QString( "COALESCE( \"%1\", '<NULL>' )" ).arg( previewAction->text() ) ) )
    {
      QMessageBox::warning( this,
                            tr( "Could not set preview column" ),
                            tr( "Could not set column '%1' as preview column.\nParser error:\n%2" )
                            .arg( previewAction->text(), mFeatureList->parserErrorString() )
                          );
    }
    else
    {
      mFeatureListPreviewButton->setDefaultAction( previewAction );
      mFeatureListPreviewButton->setPopupMode( QToolButton::InstantPopup );
    }
  }

  mDisplayExpression = mFeatureList->displayExpression();

  Q_ASSERT( previewAction );
}

int QgsDualView::featureCount()
{
  return mMasterModel->rowCount();
}

int QgsDualView::filteredFeatureCount()
{
  return mFilterModel->rowCount();
}

void QgsDualView::viewWillShowContextMenu( QMenu* menu, const QModelIndex& atIndex )
{
  if ( !menu )
  {
    return;
  }

  QgsVectorLayer* vl = mFilterModel->layer();
  QgsMapCanvas* canvas = mFilterModel->mapCanvas();
  if ( canvas && vl && vl->geometryType() != QGis::NoGeometry )
  {
    menu->addAction( tr( "Zoom to feature" ), this, SLOT( zoomToCurrentFeature() ) );
  }

  QModelIndex sourceIndex = mFilterModel->mapToSource( atIndex );

  //add user-defined actions to context menu
  if ( mLayerCache->layer()->actions()->size() != 0 )
  {

    QAction *a = menu->addAction( tr( "Run layer action" ) );
    a->setEnabled( false );

    for ( int i = 0; i < mLayerCache->layer()->actions()->size(); i++ )
    {
      const QgsAction &action = mLayerCache->layer()->actions()->at( i );

      if ( !action.runable() )
        continue;

      QgsAttributeTableAction *a = new QgsAttributeTableAction( action.name(), this, i, sourceIndex );
      menu->addAction( action.name(), a, SLOT( execute() ) );
    }
  }

  //add actions from QgsMapLayerActionRegistry to context menu
  QList<QgsMapLayerAction *> registeredActions = QgsMapLayerActionRegistry::instance()->mapLayerActions( mLayerCache->layer() );
  if ( !registeredActions.isEmpty() )
  {
    //add a separator between user defined and standard actions
    menu->addSeparator();

    QList<QgsMapLayerAction*>::iterator actionIt;
    for ( actionIt = registeredActions.begin(); actionIt != registeredActions.end(); ++actionIt )
    {
      QgsAttributeTableMapLayerAction *a = new QgsAttributeTableMapLayerAction(( *actionIt )->text(), this, ( *actionIt ), sourceIndex );
      menu->addAction(( *actionIt )->text(), a, SLOT( execute() ) );
    }
  }

  menu->addSeparator();
  QgsAttributeTableAction *a = new QgsAttributeTableAction( tr( "Open form" ), this, -1, sourceIndex );
  menu->addAction( tr( "Open form" ), a, SLOT( featureForm() ) );
}

void QgsDualView::zoomToCurrentFeature()
{
  QModelIndex currentIndex = mTableView->currentIndex();
  if ( !currentIndex.isValid() )
  {
    return;
  }

  QgsFeatureIds ids;
  ids.insert( mFilterModel->rowToId( currentIndex ) );
  QgsMapCanvas* canvas = mFilterModel->mapCanvas();
  if ( canvas )
  {
    canvas->zoomToFeatureIds( mLayerCache->layer(), ids );
  }
}

void QgsDualView::previewExpressionChanged( const QString& expression )
{
  mLayerCache->layer()->setDisplayExpression( expression );
}

void QgsDualView::featureFormAttributeChanged()
{
  mFeatureList->setCurrentFeatureEdited( true );
}

void QgsDualView::setFilteredFeatures( const QgsFeatureIds& filteredFeatures )
{
  mFilterModel->setFilteredFeatures( filteredFeatures );
}

void QgsDualView::setRequest( const QgsFeatureRequest& request )
{
  mMasterModel->setRequest( request );
}

void QgsDualView::setFeatureSelectionManager( QgsIFeatureSelectionManager* featureSelectionManager )
{
  mTableView->setFeatureSelectionManager( featureSelectionManager );
  mFeatureList->setFeatureSelectionManager( featureSelectionManager );

  if ( mFeatureSelectionManager && mFeatureSelectionManager->parent() == this )
    delete mFeatureSelectionManager;

  mFeatureSelectionManager = featureSelectionManager;
}

void QgsDualView::setAttributeTableConfig( const QgsAttributeTableConfig& config )
{
  mFilterModel->setAttributeTableConfig( config );
}

void QgsDualView::progress( int i, bool& cancel )
{
  if ( !mProgressDlg )
  {
    mProgressDlg = new QProgressDialog( tr( "Loading features..." ), tr( "Abort" ), 0, 0, this );
    mProgressDlg->setWindowTitle( tr( "Attribute table" ) );
    mProgressDlg->setWindowModality( Qt::WindowModal );
    mProgressDlg->show();
  }

  mProgressDlg->setValue( i );
  mProgressDlg->setLabelText( tr( "%1 features loaded." ).arg( i ) );

  QCoreApplication::processEvents();

  cancel = mProgressDlg && mProgressDlg->wasCanceled();
}

void QgsDualView::finished()
{
  delete mProgressDlg;
  mProgressDlg = nullptr;
}

/*
 * QgsAttributeTableAction
 */

void QgsAttributeTableAction::execute()
{
  mDualView->masterModel()->executeAction( mAction, mFieldIdx );
}

void QgsAttributeTableAction::featureForm()
{
  QgsFeatureIds editedIds;
  editedIds << mDualView->masterModel()->rowToId( mFieldIdx.row() );
  mDualView->setCurrentEditSelection( editedIds );
  mDualView->setView( QgsDualView::AttributeEditor );
}

/*
 * QgsAttributeTableMapLayerAction
 */

void QgsAttributeTableMapLayerAction::execute()
{
  mDualView->masterModel()->executeMapLayerAction( mAction, mFieldIdx );
}
