/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2010-2011, Christian Muehlhaeuser <muesli@tomahawk-player.org>
 *   Copyright 2010-2012, Jeff Mitchell <jeff@tomahawk-player.org>
 *   Copyright 2010-2011, Leo Franchi <lfranchi@kde.org>
 *
 *   Tomahawk is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Tomahawk is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Tomahawk. If not, see <http://www.gnu.org/licenses/>.
 */

#include "AlbumInfoWidget.h"
#include "ui_AlbumInfoWidget.h"

#include "audio/AudioEngine.h"
#include "ViewManager.h"
#include "database/Database.h"
#include "playlist/TreeModel.h"
#include "playlist/PlayableModel.h"
#include "playlist/AlbumItemDelegate.h"
#include "playlist/GridItemDelegate.h"
#include "Source.h"
#include "MetaPlaylistInterface.h"

#include "database/DatabaseCommand_AllTracks.h"
#include "database/DatabaseCommand_AllAlbums.h"

#include "utils/TomahawkStyle.h"
#include "utils/TomahawkUtilsGui.h"
#include "utils/Logger.h"

#include <QScrollArea>
#include <QScrollBar>

using namespace Tomahawk;


AlbumInfoWidget::AlbumInfoWidget( const Tomahawk::album_ptr& album, QWidget* parent )
    : QWidget( parent )
    , ui( new Ui::AlbumInfoWidget )
{
    QWidget* widget = new QWidget;
    ui->setupUi( widget );

    m_albumsModel = new PlayableModel( ui->albums );
    ui->albums->setPlayableModel( m_albumsModel );
    ui->albums->setEmptyTip( tr( "Sorry, we could not find any other albums for this artist!" ) );
    

    m_tracksModel = new TreeModel( ui->tracks );
    m_tracksModel->setMode( Mixed );

    QPalette trackViewPal = ui->tracks->palette();
    trackViewPal.setColor( QPalette::Foreground, TomahawkStyle::PAGE_FOREGROUND );
    trackViewPal.setColor( QPalette::Text, TomahawkStyle::PAGE_FOREGROUND );
    trackViewPal.setColor( QPalette::Highlight, QColor( "#252020" ) );
    trackViewPal.setColor( QPalette::HighlightedText, Qt::white );

    ui->tracks->setPalette( trackViewPal );
    ui->tracks->setAlternatingRowColors( false );
    ui->tracks->setRootIsDecorated( false );
    ui->tracks->setEmptyTip( tr( "Sorry, we could not find any tracks for this album!" ) );
    ui->tracks->proxyModel()->setStyle( PlayableProxyModel::Large );
    ui->tracks->setAutoResize( true );
    ui->tracks->setPlayableModel( m_tracksModel );

    AlbumItemDelegate* del = new AlbumItemDelegate( ui->tracks, ui->tracks->proxyModel() );
    ui->tracks->setPlaylistItemDelegate( del );

/*    ui->albums->setAutoFitItems( false );
    ui->albums->setWrapping( false );
    ui->albums->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    ui->albums->setHorizontalScrollBarPolicy( Qt::ScrollBarAsNeeded );*/
    ui->albums->delegate()->setItemSize( QSize( 170, 170 ) );
    ui->albums->proxyModel()->setHideDupeItems( true );

    ui->tracks->setFrameShape( QFrame::NoFrame );
    ui->tracks->setAttribute( Qt::WA_MacShowFocusRect, 0 );

    m_pixmap = TomahawkUtils::defaultPixmap( TomahawkUtils::DefaultAlbumCover, TomahawkUtils::Original, QSize( 48, 48 ) );
    ui->cover->setPixmap( TomahawkUtils::defaultPixmap( TomahawkUtils::DefaultAlbumCover, TomahawkUtils::Grid, ui->cover->size() ) );
    ui->cover->setShowText( true );

    ui->biography->setFrameShape( QFrame::NoFrame );
    ui->biography->setAttribute( Qt::WA_MacShowFocusRect, 0 );
    TomahawkStyle::styleScrollBar( ui->biography->verticalScrollBar() );

    QPalette p = ui->biography->palette();
    p.setColor( QPalette::Foreground, TomahawkStyle::PAGE_FOREGROUND );
    p.setColor( QPalette::Text, TomahawkStyle::PAGE_TEXT );

    ui->biography->setPalette( p );
    ui->label->setPalette( p );
    ui->label_2->setPalette( p );

    QScrollArea* area = new QScrollArea();
    area->setWidgetResizable( true );
    area->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
    area->setWidget( widget );

    QPalette pal = palette();
    pal.setBrush( backgroundRole(), TomahawkStyle::PAGE_BACKGROUND );
    area->setPalette( pal );
    area->setAutoFillBackground( true );
    area->setFrameShape( QFrame::NoFrame );
    area->setAttribute( Qt::WA_MacShowFocusRect, 0 );

    QVBoxLayout* layout = new QVBoxLayout();
    layout->addWidget( area );
    setLayout( layout );
    TomahawkUtils::unmarginLayout( layout );

    TomahawkStyle::styleScrollBar( ui->tracks->horizontalScrollBar() );
    TomahawkStyle::styleScrollBar( ui->albums->verticalScrollBar() );

    ui->biography->setStyleSheet( "QTextBrowser#biography { background-color: transparent; }" );

    ui->albums->setStyleSheet( "QListView { background-color: transparent; }" );
    TomahawkStyle::stylePageFrame( ui->albumFrame );

    ui->tracks->setStyleSheet( "QTreeView#tracks { background-color: transparent; }" );
    TomahawkStyle::stylePageFrame( ui->trackFrame );

    MetaPlaylistInterface* mpl = new MetaPlaylistInterface();
    mpl->addChildInterface( ui->tracks->playlistInterface() );
    mpl->addChildInterface( ui->albums->playlistInterface() );
    m_playlistInterface = playlistinterface_ptr( mpl );

    load( album );
}


AlbumInfoWidget::~AlbumInfoWidget()
{
    tDebug() << Q_FUNC_INFO;
    delete ui;
}


Tomahawk::playlistinterface_ptr
AlbumInfoWidget::playlistInterface() const
{
    return m_playlistInterface;
}


bool
AlbumInfoWidget::isBeingPlayed() const
{
    //tDebug() << Q_FUNC_INFO << "audioengine playlistInterface = " << AudioEngine::instance()->currentTrackPlaylist()->id();
    //tDebug() << Q_FUNC_INFO << "albumsView playlistInterface = " << ui->albumsView->playlistInterface()->id();
    //tDebug() << Q_FUNC_INFO << "tracksView playlistInterface = " << ui->tracksView->playlistInterface()->id();

    if ( ui->albums && ui->albums->isBeingPlayed() )
        return true;

    if ( ui->albums && ui->albums->playlistInterface() == AudioEngine::instance()->currentTrackPlaylist() )
        return true;

    if ( ui->tracks && ui->tracks->playlistInterface() == AudioEngine::instance()->currentTrackPlaylist() )
        return true;

    return false;
}


bool
AlbumInfoWidget::jumpToCurrentTrack()
{
    return  ui->albums && ui->albums->jumpToCurrentTrack();
}


void
AlbumInfoWidget::load( const album_ptr& album )
{
    if ( !m_album.isNull() )
    {
        disconnect( m_album.data(), SIGNAL( updated() ), this, SLOT( onAlbumImageUpdated() ) );
    }

    m_album = album;
    m_title = album->name();

    connect( m_album.data(), SIGNAL( updated() ), SLOT( onAlbumImageUpdated() ) );

    ui->label_2->setText( tr( "Other Albums by %1" ).arg( album->artist()->name() ) );
    ui->cover->setAlbum( album );

    m_tracksModel->startLoading();
    m_tracksModel->addTracks( album, QModelIndex(), true );
    loadAlbums( true );

    onAlbumImageUpdated();
}


void
AlbumInfoWidget::loadAlbums( bool autoRefetch )
{
    Q_UNUSED( autoRefetch );

    m_albumsModel->clear();

    connect( m_album->artist().data(), SIGNAL( albumsAdded( QList<Tomahawk::album_ptr>, Tomahawk::ModelMode ) ),
                                         SLOT( gotAlbums( QList<Tomahawk::album_ptr> ) ) );

    if ( !m_album->artist()->albums( Mixed ).isEmpty() )
        gotAlbums( m_album->artist()->albums( Mixed ) );
}


void
AlbumInfoWidget::onAlbumImageUpdated()
{
    if ( m_album->cover( QSize( 0, 0 ) ).isNull() )
        return;

    m_pixmap = m_album->cover( QSize( 0, 0 ) );
    emit pixmapChanged( m_pixmap );

    ui->cover->setPixmap( TomahawkUtils::createRoundedImage( m_album->cover( ui->cover->sizeHint() ), QSize( 0, 0 ) ) );
}


void
AlbumInfoWidget::gotAlbums( const QList<Tomahawk::album_ptr>& albums )
{
    QList<Tomahawk::album_ptr> al = albums;
    if ( al.contains( m_album ) )
        al.removeAll( m_album );

    m_albumsModel->appendAlbums( al );
}


void
AlbumInfoWidget::onArtistClicked()
{
    ViewManager::instance()->show( m_album->artist() );
}


void
AlbumInfoWidget::changeEvent( QEvent* e )
{
    QWidget::changeEvent( e );
    switch ( e->type() )
    {
        case QEvent::LanguageChange:
            ui->retranslateUi( this );
            break;

        default:
            break;
    }
}


QPixmap
AlbumInfoWidget::pixmap() const
{
    if ( m_pixmap.isNull() )
        return Tomahawk::ViewPage::pixmap();
    else
        return m_pixmap;
}
