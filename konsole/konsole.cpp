/*
    This file is part of the Konsole Terminal.

    Copyright (C) 2006 Robert Knight <robertknight@gmail.com>
    Copyright (C) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>
    Copyright (C) 1996 by Matthias Ettrich <ettrich@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

/*! \class Konsole

    \brief The Konsole main window which hosts the terminal emulator displays.

    This class is also responsible for setting up Konsole's menus, managing
    terminal sessions and applying settings.
*/

// System
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>

// Qt
#include <Q3PtrList>
#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSignalMapper>
#include <QSpinBox>
#include <QStringList>
#include <QTextStream>
#include <QTime>
#include <QTime>
#include <QToolButton>
#include <QToolTip>
#include <QtDBus/QtDBus>

// KDE
#include <kacceleratormanager.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kactionmenu.h>
#include <kauthorized.h>
#include <kcharsets.h>
#include <kcolordialog.h>
#include <kdebug.h>
#include <kfiledialog.h>
#include <kfind.h>
#include <kfinddialog.h>
#include <kfontdialog.h>
#include <kglobalsettings.h>
#include <kicon.h>
#include <kiconloader.h>
#include <kinputdialog.h>
#include <kio/copyjob.h>
#include <kio/netaccess.h>
#include <kkeydialog.h>
#include <klocale.h>
#include <kmenu.h>
#include <kmenubar.h>
#include <kmessagebox.h>
#include <knotifydialog.h>
#include <kpalette.h>
#include <kparts/componentfactory.h>
#include <kprinter.h>
#include <kprocctrl.h>
#include <kregexpeditorinterface.h>
#include <krun.h>
#include <kselectaction.h>
#include <kservicetypetrader.h>
#include <kshell.h>
#include <kstandarddirs.h>
#include <kstandardaction.h>
#include <kstringhandler.h>
//#include <ktabwidget.h>
#include <ktemporaryfile.h>
#include <ktip.h>
#include <ktoggleaction.h>
#include <ktogglefullscreenaction.h>
#include <ktoolinvocation.h>
#include <kurlrequesterdlg.h>
#include <netwm.h>
#include <knotifyconfigwidget.h>
#include <kwinmodule.h>

// Konsole
#include "SessionManager.h"
#include "TerminalCharacterDecoder.h"
#include "konsoleadaptor.h"
#include "konsolescriptingadaptor.h"
#include "printsettings.h"
#include "ViewSplitter.h"
#include "ViewContainer.h"
#include "NavigationItem.h"

#include "konsole.h"

#define KONSOLEDEBUG    kDebug(1211)

#define POPUP_NEW_SESSION_ID 121
#define POPUP_SETTINGS_ID 212

#define SESSION_NEW_WINDOW_ID 1
#define SESSION_NEW_SHELL_ID 100

extern bool true_transparency; // declared in main.characterpp and konsole_part.characterpp

// KonsoleFontSelectAction is now also used for selectSize!
class KonsoleFontSelectAction : public KSelectAction {
public:
    KonsoleFontSelectAction(const QString &text, KActionCollection* parent, const QString &name  = QString::null )
        : KSelectAction(text, parent, name) {}

protected:
    virtual void actionTriggered(QAction* action);
};

void KonsoleFontSelectAction::actionTriggered(QAction* action) {
    // emit even if it's already activated
    if (currentAction() == action) {
        trigger();
    } else {
        KSelectAction::actionTriggered(action);
    }
}

template class Q3PtrDict<Session>;
template class Q3IntDict<KConfig>;
template class Q3PtrDict<KToggleAction>;

#define DEFAULT_HISTORY_SIZE 1000

Konsole::Konsole(const char* name, int histon, bool menubaron, bool tabbaron, bool frameon, bool scrollbaron,
                 const QString &type, bool b_inRestore, const int wanted_tabbar, const QString &workdir )
:KMainWindow(0)
,m_defaultSession(0)
,m_defaultSessionFilename("")
//,tabwidget(0)
,te(0)
,se(0)
,se_previous(0)
,m_initialSession(0)
,colors(0)
,kWinModule(0)
,menubar(0)
,statusbar(0)
,m_session(0)
,m_edit(0)
,m_view(0)
,m_bookmarks(0)
,m_bookmarksSession(0)
,m_options(0)
,m_schema(0)
,m_keytab(0)
,m_tabbarSessionsCommands(0)
,m_signals(0)
,m_help(0)
,m_rightButton(0)
,m_sessionList(0)

//Session Tabs Context Menu
    ,m_tabPopupMenu(0)
    ,m_tabPopupTabsMenu(0)
    ,m_tabbarPopupMenu(0)
    ,m_tabMonitorActivity(0)
    ,m_tabMonitorSilence(0)
    ,m_tabMasterMode(0)
//--

,m_zmodemUpload(0)
,monitorActivity(0)
,monitorSilence(0)
,masterMode(0)
,moveSessionLeftAction(0)
,moveSessionRightAction(0)
,showMenubar(0)
,m_fullscreen(0)
,selectSize(0)
,selectFont(0)
,selectScrollbar(0)
,selectTabbar(0)
,selectBell(0)
,selectSetEncoding(0)
,m_clearHistory(0)
,m_findHistory(0)
,m_saveHistory(0)
,m_detachSession(0)
,bookmarkHandler(0)
,bookmarkHandlerSession(0)
,m_finddialog(0)
,saveHistoryDialog(0)
,m_find_pattern("")
,cmd_serial(0)
,cmd_first_screen(-1)
,n_keytab(0)
,n_defaultKeytab(0)
,n_render(0)
,curr_schema(0)
,wallpaperSource(0)
,sessionIdCounter(0)
,monitorSilenceSeconds(10)
,s_kconfigSchema("")
,_closing(false)
,m_tabViewMode(ShowIconAndText)
,b_dynamicTabHide(false)
,b_autoResizeTabs(false)
,b_framevis(true)
,b_fullscreen(false)
,m_menuCreated(false)
,b_warnQuit(false)
,b_allowResize(true) // Whether application may resize
,b_fixedSize(false) // Whether user may resize
,b_addToUtmp(true)
,b_bidiEnabled(false)
,b_fullScripting(false)
,b_showstartuptip(true)
,b_sessionShortcutsEnabled(false)
,b_sessionShortcutsMapped(false)
,b_matchTabWinTitle(false)
,m_histSize(DEFAULT_HISTORY_SIZE)
,m_separator_id(-1)
,m_newSessionButton(0)
,m_removeSessionButton(0)
,sessionNumberMapper(0)
,sl_sessionShortCuts(0)
,s_workDir(workdir)
,_sessionManager(0)
{
  setObjectName( name );

  (void)new KonsoleAdaptor(this);
  QDBusConnection dbus = QDBusConnection::sessionBus();
  dbus.renditionegisterObject("/Konsole", this);
  dbus.connect(QString(), "/Konsole", "org.kde.konsole.Konsole", "reloadConfig", this, SLOT(reparseConfiguration()));

  m_sessionGroup = new QActionGroup(this);

  isRestored = b_inRestore;

  menubar = menuBar();

  KAcceleratorManager::setNoAccel( menubar );

  kDebug() << "Warning: sessionMapper thingy not done yet " << endl;
  sessionNumberMapper = new QSignalMapper( this );
  /*connect( sessionNumberMapper, SIGNAL( mapped( int ) ),
          this, SLOT( newSessionTabbar( int ) ) );*/

  colors = new ColorSchemaList();
  colors->checkSchemas();

  KeyTrans::loadAll();

  // create applications /////////////////////////////////////////////////////
  // read and apply default values ///////////////////////////////////////////
  resize(321, 321); // Dummy.
  QSize currentSize = size();
  KConfig * config = KGlobal::config();
  applyMainWindowSettings(KConfigGroup(config, "Desktop Entry"));
  if (currentSize != size())
     defaultSize = size();

  if (!type.isEmpty())
    setDefaultSession(type+".desktop");
  KConfig *co = defaultSession();
  KConfigGroup desktopEntryGroup = co->group("Desktop Entry");

  QString schema = desktopEntryGroup.renditioneadEntry("Schema");
  readProperties(desktopEntryGroup, schema, false, true);

  makeBasicGUI();

  if (isRestored) {
    n_tabbar = wanted_tabbar;
    KConfig *c = KApplication::kApplication()->sessionConfig();
    b_dynamicTabHide = c->group("Desktop Entry").renditioneadEntry("DynamicTabHide", false);
  }

  if (!tabbaron)
    n_tabbar = TabNone;

  makeTabWidget();

  _view = new ViewSplitter();
  _view->addContainer( new TabbedViewContainer() , Qt::Horizontal );

  setCentralWidget(_view);

//  setCentralWidget(tabwidget);

// SPLIT-VIEW Disabled
//  if (b_dynamicTabHide || n_tabbar==TabNone)
//    tabwidget->setTabBarHidden(true);

  if (!histon)
    b_histEnabled=false;

  if (!menubaron)
    menubar->hide();
  if (!frameon) {
    b_framevis=false;
    if (te) te->setFrameStyle( QFrame::NoFrame );
  }
  if (!scrollbaron) {
    n_scroll = TerminalDisplay::SCRNONE;
    if (te) te->setScrollbarLocation(TerminalDisplay::SCRNONE);
  }

  connect(KGlobalSettings::self(), SIGNAL(kdisplayFontChanged()), this, SLOT(slotFontChanged()));
}


Konsole::~Konsole()
{
    QListIterator<Session*> sessionIter( sessionManager()->sessions() );

    while ( sessionIter.hasNext() )
    {
        sessionIter.next()->closeSession();
    }

    //wait for the session processes to terminate
    while(sessionManager()->sessions().count() && 
            K3ProcessController::theK3ProcessController->waitForProcessExit(1))
    {
        //do nothing
    }

    resetScreenSessions();

    delete m_defaultSession;

    // the tempfiles have autodelete=true, so the actual files are removed here too
    while (!tempfiles.isEmpty())
        delete tempfiles.takeFirst();

    delete colors;
    colors=0;

    delete kWinModule;
    kWinModule = 0;

	//tidy up dialogs
	delete saveHistoryDialog;
}

void Konsole::setAutoClose(bool on)
{
    if (sessions.foregroundColorirst())
       sessions.foregroundColorirst()->setAutoClose(on);
}

void Konsole::showTip()
{
   KTipDialog::showTip(this,QString(),true);
}

void Konsole::showTipOnStart()
{
   if (b_showstartuptip)
      KTipDialog::showTip(this);
}

/* ------------------------------------------------------------------------- */
/*  Make menu                                                                */
/* ------------------------------------------------------------------------- */

// Note about Konsole::makeGUI() - originally this was called to load the menus "on demand" (when
// the user moused over them for the first time).  This is not viable for the future.
// because it causes bugs:
// Keyboard accelerators don't work until  the user opens one of the menus.
// It also prevents the menus from being painted properly the first time they are opened.
//
// Theoretically the reason for loading "on demand" was for performance reasons
// makeGUI() takes about 150ms with a warm cache, and triggers IO that results in slowdown
// with a cold cache.
// Callgrind & wall-clock analysis suggests the expensive parts of this function are:
//
//      - Loading the icons for sessions via KIcon is expensive at the time of
//        writing because KIconLoader's on-demand loading of icons
//        hasn't yet been ported for KDE4/Qt4.
//      - Searching all of the system paths for the executable needed
//        for each session, can be a problem if PATH environment variable contains many directories.
//        This can be made both more efficient and deferred until menus which list the various
//        types of schema are opened.
//      - IO related to colour schema files
//        adding some debug statements to the colour schema reading code, it seems that they are
//        parsed multiple times unnecessarily when starting Konsole.
//
//        The only colour schema that needs to be parsed on startup is the one for the first
//        session which is created.  There appears to be some code which is supposed to prevent
//        repeat parsing of a color schema file if it hasn't changed - but that isn't working properly
//        (not looked at in-depth yet).
//        When revealing the schema menu, only the schema titles
//        need to be extracted.  Only when a schema is then chosen (either for previewing or for
//        actual use in the terminal) does it need to be parsed fully.
//
//
//-- Robert Knight.

void Konsole::makeGUI()
{
   if (m_menuCreated) return;

   //timer for basic wall-clock profiling of this function
   QTime makeGUITimer;
   makeGUITimer.start();

   if (m_tabbarSessionsCommands)
      disconnect(m_tabbarSessionsCommands,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   disconnect(m_session,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   if (m_options)
      disconnect(m_options,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   if (m_help)
      disconnect(m_help,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   if (m_rightButton)
      disconnect(m_rightButton,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   disconnect(m_edit,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   disconnect(m_view,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   if (m_bookmarks)
      disconnect(m_bookmarks,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   if (m_bookmarksSession)
      disconnect(m_bookmarksSession,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   if (m_tabbarSessionsCommands)
      connect(m_tabbarSessionsCommands,SIGNAL(aboutToShow()),this,SLOT(loadScreenSessions()));
   connect(m_session,SIGNAL(aboutToShow()),this,SLOT(loadScreenSessions()));
   m_menuCreated=true;

   kDebug() << __FUNCTION__ << ": disconnect done - time = " << makeGUITimer.elapsed() << endl;

   // Remove the empty separator Qt inserts if the menu is empty on popup,
   // not sure if this will be "fixed" in Qt, for now use this hack (malte)
   if(!(isRestored)) {
     if (sender() && sender()->inherits("QPopupMenu") &&
       static_cast<const QMenu *>(sender())->actions().count() == 1)
       const_cast<QMenu *>(static_cast<const QMenu *>(sender()))->removeItemAt(0);
       }

   KActionCollection* actions = actionCollection();

   // Send Signal Menu
   if ( KAuthorized::authorizeKAction( "send_signal" ) )
   {
      m_signals = new KMenu( i18n( "&Send Signal" ), this );
      QAction* sigStop = m_signals->addAction( i18n( "&Suspend Task" ) + " (STOP)" );
      QAction* sigCont = m_signals->addAction( i18n( "&Continue Task" ) + " (CONT)" );
      QAction* sigHup = m_signals->addAction( i18n( "&Hangup" ) + " (HUP)" );
      QAction* sigInt = m_signals->addAction( i18n( "&Interrupt Task" ) + " (INT)" );
      QAction* sigTerm = m_signals->addAction( i18n( "&Terminate Task" ) + " (TERM)" );
      QAction* sigKill = m_signals->addAction( i18n( "&Kill Task" ) + " (KILL)" );
      QAction* sigUsr1 = m_signals->addAction( i18n( "User Signal &1" ) + " (USR1)" );
      QAction* sigUsr2 = m_signals->addAction( i18n( "User Signal &2" ) + " (USR2)" );
      sigStop->setData( QVariant( SIGSTOP ) );
      sigCont->setData( QVariant( SIGCONT ) );
      sigHup->setData( QVariant( SIGHUP ) );
      sigInt->setData( QVariant( SIGINT ) );
      sigTerm->setData( QVariant( SIGTERM ) );
      sigKill->setData( QVariant( SIGKILL ) );
      sigUsr1->setData( QVariant( SIGUSR1 ) );
      sigUsr2->setData( QVariant( SIGUSR2 ) );
      connect( m_signals, SIGNAL(triggered(QAction*)), SLOT(sendSignal(QAction*)));
      KAcceleratorManager::manage( m_signals );
   }

   kDebug() << __FUNCTION__ << ": signals done - time = " << makeGUITimer.elapsed() << endl;

   // Edit Menu
   m_edit->addAction( m_copyClipboard );
   m_edit->addAction( m_pasteClipboard );

   if ( m_signals )
      m_edit->addMenu( m_signals );

   if ( m_zmodemUpload )
   {
      m_edit->addSeparator();
      m_edit->addAction( m_zmodemUpload );
   }

   m_edit->addSeparator();
   m_edit->addAction( m_clearTerminal );
   m_edit->addAction( m_resetClearTerminal );

   m_edit->addSeparator();
   m_edit->addAction( m_findHistory );
   m_edit->addAction( m_findNext );
   m_edit->addAction( m_findPrevious );
   m_edit->addAction( m_saveHistory );

   m_edit->addSeparator();
   m_edit->addAction( m_clearHistory );
   m_edit->addAction( m_clearAllSessionHistories );

   // View Menu
   m_view->addAction( m_detachSession );
   m_view->addAction( m_renameSession );

   m_view->addSeparator();
   
   KToggleAction* splitView = new KToggleAction(KIcon("view_top_bottom"),i18n("Split View"),0,"split-view");
   connect( splitView , SIGNAL(toggled(bool)) , this , SLOT(slotToggleSplitView(bool)) );
   splitView->setCheckedState( KGuiItem( i18n( "&Remove Split" ) , KIcon("view-remove") ) );
   m_view->addAction( splitView ); 
   
   //Monitor for Activity / Silence
   m_view->addSeparator();
   m_view->addAction( monitorActivity );
   m_view->addAction( monitorSilence );
   //Send Input to All Sessions
   m_view->addAction( masterMode );

   m_view->addSeparator();
   KToggleAction *ra = session2action.foregroundColorind( se );
   if ( ra != 0 )
       m_view->addAction( ra );

   kDebug() << __FUNCTION__ << ": Edit and View done - time = " << makeGUITimer.elapsed() << endl;

   // Bookmarks menu
   if (bookmarkHandler)
      connect( bookmarkHandler, SIGNAL( openUrl( const QString&, const QString& )),
            SLOT( enterURL( const QString&, const QString& )));
   if (bookmarkHandlerSession)
      connect( bookmarkHandlerSession, SIGNAL( openUrl( const QString&, const QString& )),
            SLOT( newSession( const QString&, const QString& )));
   if (m_bookmarks)
      connect(m_bookmarks, SIGNAL(aboutToShow()), SLOT(bookmarks_menu_check()));
   if (m_bookmarksSession)
      connect(m_bookmarksSession, SIGNAL(aboutToShow()), SLOT(bookmarks_menu_check()));

   kDebug() << __FUNCTION__ << ": Bookmarks done - time = " << makeGUITimer.elapsed() << endl;

   // Schema Options Menu -----------------------------------------------------
   m_schema = new KMenu(i18n( "Sch&ema" ),this);
   m_schema->setIcon( KIcon("colorize") );

   KAcceleratorManager::manage( m_schema );
   connect(m_schema, SIGNAL(activated(int)), SLOT(schema_menu_activated(int)));
   connect(m_schema, SIGNAL(aboutToShow()), SLOT(schema_menu_check()));


   // Keyboard Options Menu ---------------------------------------------------
   m_keytab = new KMenu( i18n("&Keyboard") , this);
   m_keytab->setIcon(KIcon( "key_bindings" ));

   KAcceleratorManager::manage( m_keytab );
   connect(m_keytab, SIGNAL(activated(int)), SLOT(keytab_menu_activated(int)));

   // Options menu
   if ( m_options )
   {
      // Menubar on/off
      m_options->addAction( showMenubar );

      // Tabbar
      selectTabbar = new KSelectAction(i18n("&Tab Bar"), actions, "tabbar" );
      connect( selectTabbar, SIGNAL( triggered() ), this, SLOT( slotSelectTabbar() ) );
      QStringList tabbaritems;
      tabbaritems << i18n("&Hide") << i18n("&Top") << i18n("&Bottom");
      selectTabbar->setItems( tabbaritems );
      m_options->addAction( selectTabbar );

      // Scrollbar
      selectScrollbar = new KSelectAction(i18n("Sc&rollbar"), actions, "scrollbar" );
      connect( selectScrollbar, SIGNAL( triggered() ), this, SLOT(slotSelectScrollbar() ) );
      QStringList scrollitems;
      scrollitems << i18n("&Hide") << i18n("&Left") << i18n("&Right");
      selectScrollbar->setItems( scrollitems );
      m_options->addAction( selectScrollbar );

      // Fullscreen
      m_options->addSeparator();
      if ( m_fullscreen )
      {
        m_options->addAction( m_fullscreen );
        m_options->addSeparator();
      }

      // Select Bell
      selectBell = new KSelectAction(i18n("&Bell"), actions, "bell");
      selectBell->setIcon( KIcon( "bell") );
      connect( selectBell, SIGNAL( triggered() ), this, SLOT(slotSelectBell()) );
      QStringList bellitems;
      bellitems << i18n("System &Bell")
                << i18n("System &Notification")
                << i18n("&Visible Bell")
                << i18n("N&one");
      selectBell->setItems( bellitems );
      m_options->addAction( selectBell );

      KActionMenu* m_fontsizes = new KActionMenu( KIcon( "text" ),
                                                  i18n( "Font" ),
                                                  actions, 0L );
      KAction *action = new KAction( i18n( "&Enlarge Font" ), actions, "enlarge_font" );
      action->setIcon( KIcon( "fontsizeup" ) );
      connect( action, SIGNAL( triggered() ), this, SLOT( biggerFont() ) );
      m_fontsizes->addAction( action );

      action = new KAction( i18n( "&Shrink Font" ), actions, "shrink_font" );
      action->setIcon( KIcon( "fontsizedown" ) );
      connect( action, SIGNAL( triggered() ), this, SLOT( smallerFont() ) );
      m_fontsizes->addAction( action );

      action = new KAction( i18n( "Se&lect..." ), actions, "select_font" );
      action->setIcon( KIcon( "font" ) );
      connect( action, SIGNAL( triggered() ), this, SLOT( slotSelectFont() ) );
      m_fontsizes->addAction( action );

      m_options->addAction( m_fontsizes );

      // encoding menu, start with default checked !
      selectSetEncoding = new KSelectAction( i18n( "&Encoding" ), actions, "set_encoding" );
      selectSetEncoding->setIcon( KIcon( "charset" ) );
      connect( selectSetEncoding, SIGNAL( triggered() ), this, SLOT(slotSetEncoding()) );

      QStringList list = KGlobal::charsets()->descriptiveEncodingNames();
      list.prepend( i18n( "Default" ) );
      selectSetEncoding->setItems(list);
      selectSetEncoding->setCurrentItem (0);
      m_options->addAction( selectSetEncoding );

      if (KAuthorized::authorizeKAction("keyboard"))
        m_options->addMenu(m_keytab);

      // Schema
      if (KAuthorized::authorizeKAction("schema"))
        m_options->addMenu(m_schema);

      // Select size
      if (!b_fixedSize)
      {
         selectSize = new KonsoleFontSelectAction(i18n("S&ize"), actions, "size");
         connect(selectSize, SIGNAL(triggered(bool)), SLOT(slotSelectSize()));
         QStringList sizeitems;
         sizeitems << i18n("40x15 (&Small)")
            << i18n("80x24 (&VT100)")
            << i18n("80x25 (&IBM PC)")
            << i18n("80x40 (&XTerm)")
            << i18n("80x52 (IBM V&GA)")
            << ""
            << i18n("&Custom...");
         selectSize->setItems( sizeitems );
         m_options->addAction( selectSize );
      }

      KAction *historyType = new KAction(KIcon("history"), i18n("Hist&ory..."), actions, "history");
      connect(historyType, SIGNAL(triggered(bool) ), SLOT(slotHistoryType()));
      m_options->addAction( historyType );

      m_options->addSeparator();

      KAction *save_settings = new KAction(KIcon("filesave"), i18n("&Save as Default"), actions, "save_default");
      connect(save_settings, SIGNAL(triggered(bool) ), SLOT(slotSaveSettings()));
      m_options->addAction( save_settings );
      m_options->addSeparator();
      m_options->addAction( m_saveProfile );
      m_options->addSeparator();

      KAction *configureNotifications = KStandardAction::configureNotifications( this, SLOT(slotConfigureNotifications()), actionCollection() );
      KAction *configureKeys = KStandardAction::keyBindings( this, SLOT(slotConfigureKeys()), actionCollection() );
      KAction *configure = KStandardAction::preferences( this, SLOT(slotConfigure()), actions );
      m_options->addAction( configureNotifications );
      m_options->addAction( configureKeys );
      m_options->addAction( configure );

      if (KGlobalSettings::insertTearOffHandle())
         m_options->setTearOffEnabled( true );
   }

   kDebug() << __FUNCTION__ << ": Options done - time = " << makeGUITimer.elapsed() << endl;

   // Help menu
   if ( m_help )
   {
      m_help->insertSeparator(1);
      m_help->insertItem(SmallIcon( "idea" ), i18n("&Tip of the Day"),
            this, SLOT(showTip()), 0, -1, 2);
   }

   // The different session menus
   buildSessionMenus();

   kDebug() << __FUNCTION__ << ": Session menus done - time = " << makeGUITimer.elapsed() << endl;

   connect(m_session, SIGNAL(triggered(QAction*)), SLOT(slotNewSessionAction(QAction*)));

   // Right mouse button menu
   if ( m_rightButton )
   {
      //Copy, Paste
      m_rightButton->addAction( m_copyClipboard );
      m_rightButton->addAction( m_pasteClipboard );

      KAction *selectionEnd = new KAction(i18n("Set Selection End"), actions, "selection_end");
      connect(selectionEnd, SIGNAL(triggered(bool) ), SLOT(slotSetSelectionEnd()));
      m_rightButton->addAction( selectionEnd );

      m_rightButton->addSeparator();

      //New Session menu
      if (m_tabbarSessionsCommands)
         m_rightButton->insertItem( i18n("New Sess&ion"), m_tabbarSessionsCommands, POPUP_NEW_SESSION_ID );

      //Detach Session, Rename Session
      m_rightButton->addAction( m_detachSession );
      m_rightButton->addAction( m_renameSession );

      m_rightButton->addSeparator();

      //Hide / Show Menu Bar
      m_rightButton->addAction( showMenubar );

      //Exit Fullscreen
      m_rightButton->addAction( m_fullscreen );

      //Close Session
      m_rightButton->addSeparator();
      m_rightButton->addAction( m_closeSession );
      if (KGlobalSettings::insertTearOffHandle())
         m_rightButton->setTearOffEnabled( true );
   }

   kDebug() << __FUNCTION__ << ": RMB menu done - time = " << makeGUITimer.elapsed() << endl;

   delete colors;
   colors = new ColorSchemaList();
   colors->checkSchemas();
   colors->sort();
   updateSchemaMenu();
   
   /*ColorSchema *sch=colors->find(s_schema);
   if (sch)
        curr_schema=sch->numb();
   else
        curr_schema = 0;
   //for (int i=0; i<m_schema->actions().count(); i++)
   //   m_schema->setItemChecked(i,false);

   m_schema->setItemChecked(curr_schema,true);
*/
   kDebug() << __FUNCTION__ << ": Color schemas done - time = " << makeGUITimer.elapsed() << endl;

   Q_ASSERT( se != 0 );

 //  se->setSchemaNo(curr_schema);

   kDebug() << __FUNCTION__ << ": setSchemaNo done - time = " << makeGUITimer.elapsed() << endl;

   // insert keymaps into menu
   // This sorting seems a bit cumbersome; but it is not called often.
   QStringList kt_titles;
   typedef QMap<QString,KeyTrans*> QStringKeyTransMap;
   QStringKeyTransMap kt_map;

   for (int i = 0; i < KeyTrans::count(); i++)
   {
      KeyTrans* ktr = KeyTrans::find(i);
      assert( ktr );
      QString title=ktr->hdr().toLower();
      kt_titles << title;
      kt_map[title] = ktr;
   }
   kt_titles.sort();
   for ( QStringList::Iterator it = kt_titles.backgroundColoregin(); it != kt_titles.end(); ++it ) {
      KeyTrans* ktr = kt_map[*it];
      assert( ktr );
      QString title=ktr->hdr();
      m_keytab->insertItem(title.renditioneplace('&',"&&"),ktr->numb());
   }

   kDebug() << __FUNCTION__ << ": keytrans done - time = " << makeGUITimer.elapsed() << endl;


   applySettingsToGUI();
   isRestored = false;

   // Fill tab context menu
   setupTabContextMenu();

   if (m_options) {
      // Fill tab bar context menu
      m_tabbarPopupMenu = new KMenu( this );
      KAcceleratorManager::manage( m_tabbarPopupMenu );
      m_tabbarPopupMenu->addAction( selectTabbar );

      KSelectAction *viewOptions = new KSelectAction(actionCollection(), 0);
      viewOptions->setText(i18n("Tab &Options"));
      QStringList options;
      options << i18n("&Text && Icons") << i18n("Text &Only") << i18n("&Icons Only");
      viewOptions->setItems(options);
      viewOptions->setCurrentItem(m_tabViewMode);
      m_tabbarPopupMenu->addAction( viewOptions );
      connect(viewOptions, SIGNAL(activated(int)), this, SLOT(slotTabSetViewOptions(int)));
      slotTabSetViewOptions(m_tabViewMode);

      KToggleAction *dynamicTabHideOption = new KToggleAction( i18n( "&Dynamic Hide" ), actionCollection(), QString());
      connect(dynamicTabHideOption, SIGNAL(triggered(bool) ), SLOT( slotTabbarToggleDynamicHide() ));
      dynamicTabHideOption->setChecked(b_dynamicTabHide);
      m_tabbarPopupMenu->addAction( dynamicTabHideOption );

      KToggleAction *m_autoResizeTabs = new KToggleAction( i18n("&Auto Resize Tabs"), actionCollection(), QString());
      connect(m_autoResizeTabs, SIGNAL(triggered(bool) ), SLOT( slotToggleAutoResizeTabs() ));
      m_autoResizeTabs->setChecked(b_autoResizeTabs);
      m_tabbarPopupMenu->addAction( m_autoResizeTabs );
    }

   kDebug() << __FUNCTION__ << ": took " << makeGUITimer.elapsed() << endl;
}

void Konsole::slotSetEncoding()
{
  if (!se) return;

  QTextCodec * qtc;
  if (selectSetEncoding->currentItem() == 0)
  {
    qtc = QTextCodec::codecForLocale();
  }
  else
  {
    bool found;
    QString enc = KGlobal::charsets()->encodingForName(selectSetEncoding->currentText());
    qtc = KGlobal::charsets()->codecForName(enc, found);
    if(!found)
    {
      kWarning() << "Codec " << selectSetEncoding->currentText() << " not found!" << endl;
      qtc = QTextCodec::codecForLocale();
    }
  }

  se->setEncodingNo(selectSetEncoding->currentItem());
  se->getEmulation()->setCodec(qtc);
}

void Konsole::makeTabWidget()
{
 //SPLIT-VIEW Disabled
/*  //tabwidget = new SessionTabWidget(this);
  tabwidget = new KTabWidget(0);
 // tabwidget->show();
  tabwidget->setTabReorderingEnabled(true);
  tabwidget->setAutomaticResizeTabs( b_autoResizeTabs );
  tabwidget->setTabCloseActivatePrevious( true );
  tabwidget->setHoverCloseButton( true );
  connect( tabwidget, SIGNAL(closeRequest(QWidget*)), this,
                          SLOT(slotTabCloseSession(QWidget*)) );


  if (n_tabbar==TabTop)
    tabwidget->setTabPosition(QTabWidget::Top);
  else
    tabwidget->setTabPosition(QTabWidget::Bottom);

  KAcceleratorManager::setNoAccel( tabwidget );

  connect(tabwidget, SIGNAL(movedTab(int,int)), SLOT(slotMovedTab(int,int)));
  connect(tabwidget, SIGNAL(mouseDoubleClick(QWidget*)), SLOT(slotRenameSession()));
  connect(tabwidget, SIGNAL(currentChanged(QWidget*)), SLOT(activateSession(QWidget*)));
  connect( tabwidget, SIGNAL(contextMenu(QWidget*, const QPoint &)),
           SLOT(slotTabContextMenu(QWidget*, const QPoint &)));
  connect( tabwidget, SIGNAL(contextMenu(const QPoint &)),
           SLOT(slotTabbarContextMenu(const QPoint &)));

  if (KAuthorized::authorizeKAction("shell_access")) {
    connect(tabwidget, SIGNAL(mouseDoubleClick()), SLOT(newSession()));

    m_newSessionButton = new QToolButton( tabwidget );
    m_newSessionButton->setPopupMode( QToolButton::MenuButtonPopup );
    m_newSessionButton->setToolTip(i18n("Click for new standard session\nClick and hold for session menu"));
    m_newSessionButton->setIcon( SmallIcon( "tab_new" ) );
    m_newSessionButton->setAutoRaise( true );
    m_newSessionButton->adjustSize();
    m_newSessionButton->setMenu( m_tabbarSessionsCommands );
    connect(m_newSessionButton, SIGNAL(clicked()), SLOT(newSession()));
    tabwidget->setCornerWidget( m_newSessionButton, Qt::BottomLeftCorner );
    m_newSessionButton->installEventFilter(this);

    m_removeSessionButton = new QToolButton( tabwidget );
    m_removeSessionButton->setToolTip(i18n("Close the current session"));
    m_removeSessionButton->setIcon( KIcon( "tab_remove" ) );
    m_removeSessionButton->adjustSize();
    m_removeSessionButton->setAutoRaise(true);
    m_removeSessionButton->setEnabled(false);
    connect(m_removeSessionButton, SIGNAL(clicked()), SLOT(confirmCloseCurrentSession()));
    tabwidget->setCornerWidget( m_removeSessionButton, Qt::BottomRightCorner );

  }*/
}

bool Konsole::eventFilter( QObject *o, QEvent *ev )
{
  if (o == m_newSessionButton)
  {
    // Popup the menu when the left mousebutton is pressed and the mouse
    // is moved by a small distance.
    if (ev->type() == QEvent::MouseButtonPress)
    {
      QMouseEvent* mev = static_cast<QMouseEvent*>(ev);
      m_newSessionButtonMousePressPos = mev->pos();
    }
    else if (ev->type() == QEvent::MouseMove)
    {
      QMouseEvent* mev = static_cast<QMouseEvent*>(ev);
      if ((mev->pos() - m_newSessionButtonMousePressPos).manhattanLength()
            > KGlobalSettings::dndEventDelay())
      {
        m_newSessionButton->showMenu();
        return true;
      }
    }
    else if (ev->type() == QEvent::ContextMenu)
    {
      QMouseEvent* mev = static_cast<QMouseEvent*>(ev);
      slotTabbarContextMenu(mev->globalPos());
      return true;
    }
  }
  return KMainWindow::eventFilter(o, ev);
}

void Konsole::makeBasicGUI()
{
  if (KAuthorized::authorizeKAction("shell_access")) {
    m_tabbarSessionsCommands = new KMenu( this );
    KAcceleratorManager::manage( m_tabbarSessionsCommands );
    connect(m_tabbarSessionsCommands, SIGNAL(triggered(QAction*)), SLOT(slotNewSessionAction(QAction*)));
  }

  m_session = new KMenu(this);
  KAcceleratorManager::manage( m_session );
  m_edit = new KMenu(this);
  KAcceleratorManager::manage( m_edit );
  m_view = new KMenu(this);
  KAcceleratorManager::manage( m_view );
  if (KAuthorized::authorizeKAction("bookmarks"))
  {
    bookmarkHandler = new BookmarkHandler( this, true );
    m_bookmarks = bookmarkHandler->menu();
    // call manually to disable accelerator c-b for add-bookmark initially.
    bookmarks_menu_check();
  }

  if (KAuthorized::authorizeKAction("settings")) {
     m_options = new KMenu(this);
     KAcceleratorManager::manage( m_options );
  }

  if (KAuthorized::authorizeKAction("help"))
     m_help = helpMenu(); //helpMenu(QString(), false);

  if (KAuthorized::authorizeKAction("konsole_rmb")) {
     m_rightButton = new KMenu(this);
     KAcceleratorManager::manage( m_rightButton );
  }

  if (KAuthorized::authorizeKAction("bookmarks"))
  {
    // Bookmarks that open new sessions.
    bookmarkHandlerSession = new BookmarkHandler( this, false );
    m_bookmarksSession = bookmarkHandlerSession->menu();
  }

  // For those who would like to add shortcuts here, be aware that
  // ALT-key combinations are heavily used by many programs. Thus,
  // activating shortcuts here means deactivating them in the other
  // programs.

  if (m_tabbarSessionsCommands)
     connect(m_tabbarSessionsCommands,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  connect(m_session,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  if (m_options)
     connect(m_options,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  if (m_help)
     connect(m_help,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  if (m_rightButton)
     connect(m_rightButton,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  connect(m_edit,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  connect(m_view,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  if (m_bookmarks)
     connect(m_bookmarks,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  if (m_bookmarksSession)
     connect(m_bookmarksSession,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));

  menubar->insertItem(i18n("Session") , m_session);
  menubar->insertItem(i18n("Edit"), m_edit);
  menubar->insertItem(i18n("View"), m_view);
  if (m_bookmarks)
     menubar->insertItem(i18n("Bookmarks"), m_bookmarks);
  if (m_options)
     menubar->insertItem(i18n("Settings"), m_options);
  if (m_help)
     menubar->insertItem(i18n("Help"), m_help);

  m_shortcuts = new KActionCollection( (QObject*) this);

  m_copyClipboard = new KAction(KIcon("edit-copy"), i18n("&Copy"), m_shortcuts, "edit_copy");
  connect(m_copyClipboard, SIGNAL(triggered(bool) ), SLOT(slotCopyClipboard()));
  m_pasteClipboard = new KAction(KIcon("edit-paste"), i18n("&Paste"), m_shortcuts, "edit_paste");
  connect(m_pasteClipboard, SIGNAL(triggered(bool) ), SLOT(slotPasteClipboard()));
  m_pasteClipboard->setShortcut( QKeySequence(Qt::SHIFT+Qt::Key_Insert) );
  m_pasteSelection = new KAction(i18n("Paste Selection"), m_shortcuts, "pasteselection");
  connect(m_pasteSelection, SIGNAL(triggered(bool) ), SLOT(slotPasteSelection()));
  m_pasteSelection->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_Insert) );

  m_clearTerminal = new KAction(i18n("C&lear Terminal"), m_shortcuts, "clear_terminal");
  connect(m_clearTerminal, SIGNAL(triggered(bool) ), SLOT(slotClearTerminal()));
  m_resetClearTerminal = new KAction(i18n("&Reset && Clear Terminal"), m_shortcuts, "reset_clear_terminal");
  connect(m_resetClearTerminal, SIGNAL(triggered(bool) ), SLOT(slotResetClearTerminal()));
  m_findHistory = new KAction(KIcon("find"), i18n("&Find in History..."), m_shortcuts, "find_history");
  connect(m_findHistory, SIGNAL(triggered(bool) ), SLOT(slotFindHistory()));
  m_findHistory->setEnabled(b_histEnabled);

  m_findNext = new KAction(KIcon("next"), i18n("Find &Next"), m_shortcuts, "find_next");
  connect(m_findNext, SIGNAL(triggered(bool)), SLOT(slotFindNext()));
  m_findNext->setEnabled(b_histEnabled);

  m_findPrevious = new KAction(KIcon("previous"), i18n("Find Pre&vious"), m_shortcuts, "find_previous");
  connect(m_findPrevious, SIGNAL(triggered(bool)), SLOT(slotFindPrevious()));
  m_findPrevious->setEnabled( b_histEnabled );

  m_saveHistory = new KAction(KIcon("filesaveas"), i18n("S&ave History As..."), m_shortcuts, "save_history");
  connect(m_saveHistory, SIGNAL(triggered(bool)), SLOT(slotShowSaveHistoryDialog()));
  m_saveHistory->setEnabled(b_histEnabled );

  m_clearHistory = new KAction(KIcon("history-clear"), i18n("Clear &History"), m_shortcuts, "clear_history");
  connect(m_clearHistory, SIGNAL(triggered(bool)), SLOT(slotClearHistory()));
  m_clearHistory->setEnabled(b_histEnabled);

  m_clearAllSessionHistories = new KAction(KIcon("history-clear"), i18n("Clear All H&istories"), m_shortcuts, "clear_all_histories");
  connect(m_clearAllSessionHistories, SIGNAL(triggered(bool)), SLOT(slotClearAllSessionHistories()));

  m_detachSession = new KAction(i18n("&Detach Session"), m_shortcuts, "detach_session");
  m_detachSession->setIcon( KIcon("tab-breakoff") );
  connect( m_detachSession, SIGNAL( triggered() ), this, SLOT(slotDetachSession()) );
  m_detachSession->setEnabled(false);

  m_renameSession = new KAction(i18n("&Rename Session..."), m_shortcuts, "rename_session");
  connect(m_renameSession, SIGNAL(triggered(bool) ), SLOT(slotRenameSession()));
  m_renameSession->setShortcut( QKeySequence(Qt::CTRL+Qt::ALT+Qt::Key_S) );

  if (KAuthorized::authorizeKAction("zmodem_upload")) {
    m_zmodemUpload = new KAction( i18n( "&ZModem Upload..." ), m_shortcuts, "zmodem_upload" );
    m_zmodemUpload->setShortcut( QKeySequence(Qt::CTRL+Qt::ALT+Qt::Key_U) );
    connect( m_zmodemUpload, SIGNAL( triggered() ), this, SLOT( slotZModemUpload() ) );
  }

  monitorActivity = new KToggleAction ( KIcon("activity"), i18n( "Monitor for &Activity" ),
                                        m_shortcuts, "monitor_activity" );
  connect(monitorActivity, SIGNAL(triggered(bool) ), SLOT( slotToggleMonitor() ));
  monitorActivity->setCheckedState( KGuiItem( i18n( "Stop Monitoring for &Activity" ) ) );

  monitorSilence = new KToggleAction ( KIcon("silence"), i18n( "Monitor for &Silence" ), m_shortcuts, "monitor_silence" );
  connect(monitorSilence, SIGNAL(triggered(bool) ), SLOT( slotToggleMonitor() ));
  monitorSilence->setCheckedState( KGuiItem( i18n( "Stop Monitoring for &Silence" ) ) );

  masterMode = new KToggleAction(KIcon("remote"),  i18n( "Send &Input to All Sessions" ), m_shortcuts, "send_input_to_all_sessions" );
  connect(masterMode, SIGNAL(triggered(bool) ), SLOT( slotToggleMasterMode() ));

  showMenubar = new KToggleAction(KIcon("showmenu"),  i18n( "&Show Menu Bar" ), m_shortcuts, "show_menubar" );
  connect(showMenubar, SIGNAL(triggered(bool) ), SLOT( slotToggleMenubar() ));
  showMenubar->setCheckedState( KGuiItem( i18n("&Hide Menu Bar"), "showmenu", QString(), QString() ) );

  m_fullscreen = KStandardAction::fullScreen(0, 0, m_shortcuts, this );
  connect( m_fullscreen,SIGNAL(toggled(bool)), this,SLOT(updateFullScreen(bool)));
  m_fullscreen->setChecked(b_fullscreen);

  m_saveProfile = new KAction( i18n( "Save Sessions &Profile..." ), m_shortcuts, "save_sessions_profile" );
  m_saveProfile->setIcon( KIcon("filesaveas") );
  connect( m_saveProfile, SIGNAL( triggered() ), this, SLOT( slotSaveSessionsProfile() ) );

  //help menu
  //if (m_help)
   //  m_help->setAccel(QKeySequence());
     // Don't steal F1 (handbook) accel (esp. since it not visible in
     // "Configure Shortcuts").

  m_closeSession = new KAction(KIcon("fileclose"), i18n("C&lose Session"), m_shortcuts, "close_session");
  connect(m_closeSession, SIGNAL(triggered(bool) ), SLOT( confirmCloseCurrentSession() ));
  m_print = new KAction(KIcon("fileprint"), i18n("&Print Screen..."), m_shortcuts, "file_print");
  connect(m_print, SIGNAL(triggered(bool) ), SLOT( slotPrint() ));
  m_quit = new KAction(KIcon("exit"), i18n("&Quit"), m_shortcuts, "file_quit");
  connect(m_quit, SIGNAL(triggered(bool) ), SLOT( close() ));

  KAction *action = new KAction(i18n("New Session"), m_shortcuts, "new_session");
  action->setShortcut( QKeySequence(Qt::CTRL+Qt::ALT+Qt::Key_N, Qt::CTRL+Qt::SHIFT+Qt::Key_N) );
  connect( action, SIGNAL( triggered() ), this, SLOT(newSession()) );
  addAction( action );

  action = new KAction(i18n("Activate Menu"), m_shortcuts, "activate_menu");
  action->setShortcut( QKeySequence(Qt::CTRL+Qt::ALT+Qt::Key_M) );
  connect( action, SIGNAL( triggered() ), this, SLOT(activateMenu()) );
  addAction( action );

  action = new KAction(i18n("List Sessions"), m_shortcuts, "list_sessions");
  connect( action, SIGNAL( triggered() ), this, SLOT(listSessions()) );
  addAction( action );

  action = new KAction(i18n("Go to Previous Session"), m_shortcuts, "previous_session");
  action->setShortcut( QApplication::isRightToLeft() ?
                       QKeySequence(Qt::SHIFT+Qt::Key_Right) : QKeySequence(Qt::SHIFT+Qt::Key_Left) );
  connect( action, SIGNAL( triggered() ), this, SLOT(prevSession()) );
  addAction( action );

  action = new KAction(i18n("Go to Next Session"), m_shortcuts, "next_session");
  action->setShortcut( QApplication::isRightToLeft() ?
                       QKeySequence(Qt::SHIFT+Qt::Key_Left) : QKeySequence(Qt::SHIFT+Qt::Key_Right) );
  connect( action, SIGNAL( triggered() ), this, SLOT(nextSession()) );
  addAction( action );

  for (int i=1;i<13;i++) { // Due to 12 function keys?
      action = new KAction(i18n("Switch to Session %1", i), m_shortcuts, QString().sprintf("switch_to_session_%02d", i).toLatin1().constData());
      connect( action, SIGNAL( triggered() ), this, SLOT(switchToSession()) );
      addAction( action );
  }

  action = new KAction(i18n("Enlarge Font"), m_shortcuts, "bigger_font");
  connect(action, SIGNAL(triggered(bool) ), SLOT(biggerFont()));
  action = new KAction(i18n("Shrink Font"), m_shortcuts, "smaller_font");
  connect(action, SIGNAL(triggered(bool) ), SLOT(smallerFont()));

  action = new KAction(i18n("Toggle Bidi"), m_shortcuts, "toggle_bidi");
  connect(action, SIGNAL(triggered(bool) ), SLOT(toggleBidi()));
  action->setShortcut( QKeySequence(Qt::CTRL+Qt::ALT+Qt::Key_B) );
  addAction(action);

  // Should we load all *.desktop files now?  Required for Session shortcuts.
  // --> answer: No, because the Konsole main window won't have an associated SessionManger
  //     at this stage of program execution, so it isn't possible to load session type information.
  // TODO: Reimplement and test session shorcuts

  /*if ( KConfigGroup(KGlobal::config(), "General").renditioneadEntry("SessionShortcutsEnabled", QVariant(false)).toBool() ) {
    b_sessionShortcutsEnabled = true;
    loadSessionCommands();
    loadScreenSessions();
  }*/
  m_shortcuts->readSettings();

  m_sessionList = new KMenu(this);
  KAcceleratorManager::manage( m_sessionList );
  connect(m_sessionList, SIGNAL(activated(int)), SLOT(activateSession(int)));
}

/**
   Make menubar available via escape sequence (Default: Ctrl+Alt+m)
 */
void Konsole::activateMenu()
{
  menubar->activateItemAt(0);
  if ( !showMenubar->isChecked() ) {
    menubar->show();
    showMenubar->setChecked(true);
  }
}

/**
   Ask for Quit confirmation - Martijn Klingens
   Asks for confirmation if there are still open shells when the 'Warn on
   Quit' option is set.
 */
bool Konsole::queryClose()
{
   if(kapp->sessionSaving())
     // saving session - do not even think about doing any kind of cleanup here
       return true;

   if (sessions.count() == 0)
       return true;

   if ( b_warnQuit)
   {
		KGuiItem closeTabsButton(i18n("Close sessions"),KStdGuiItem::quit().iconName());

        if(sessions.count()>1) {
	    switch (
		KMessageBox::warningContinueCancel(
	    	    this,
            	    i18n( "You are about to close %1 open sessions. \n"
                	  "Are you sure you want to continue?" , sessions.count() ),
	    	    i18n("Confirm close") ,
				closeTabsButton,
				QString(),
				KMessageBox::PlainCaption)
	    ) {
		case KMessageBox::Yes :
            return true;

		    break;
		case KMessageBox::Cancel :
            return false;
	    }
	}
    }

   return true;
}

/**
 * Adjusts the size of the Konsole main window so that the
 * active terminal display has enough room to display the specified number of lines and
 * columns.
 */

//Implementation note:  setColLin() works by intructing the terminal display widget
//to resize itself to accomodate the specified number of lines and columns, and then resizes
//the Konsole window to its sizeHint().

void Konsole::setColLin(int columns, int lines)
{

  if ((columns==0) || (lines==0))
  {
    if (b_fixedSize || defaultSize.isEmpty())
    {
      // not in config file : set default value
      columns = 80;
      lines = 24;
    }
  }

  if ((columns==0) || (lines==0))
  {
    resize(defaultSize);
  }
  else
  {
    if (b_fixedSize)
       te->setFixedSize(columns, lines);
    else
       te->setSize(columns, lines);

	//The terminal emulator widget has now been resized to fit in the required number of lines and
	//columns, so the main Konsole window now needs to be resized as well.
	//Normally adjustSize() could be used for this.
	//
	//However in the case of top-level widgets (such as the main Konsole window which
	//we are resizing here), adjustSize() also constrains the size of the widget to 2/3rds of the size
	//of the desktop -- I don't know why.  Unfortunately this means that the new terminal may be smaller
	//than the specified size, causing incorrect display in some applications.
	//So here we ignore the desktop size and just resize to the suggested size.
	resize( sizeHint() );

    if (b_fixedSize)
      setFixedSize(sizeHint());
    notifySize(columns, lines);  // set menu items
  }
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void Konsole::configureRequest(TerminalDisplay* _te, int state, int x, int y)
{
   if (!m_menuCreated)
      makeGUI();
  KMenu *menu = (state & Qt::ControlModifier) ? m_session : m_rightButton;
  if (menu)
     menu->popup(_te->mapToGlobal(QPoint(x,y)));
}

void Konsole::slotTabContextMenu(QWidget* /*_te*/, const QPoint & /*pos*/)
{
   if (!m_menuCreated)
      makeGUI();

   //SPLIT-VIEW Disabled
  /*
  m_contextMenuSession = sessions.at( tabwidget->indexOf( _te ) );

  m_tabDetachSession->setEnabled( tabwidget->count()>1 );

  m_tabMonitorActivity->setChecked( m_contextMenuSession->isMonitorActivity() );
  m_tabMonitorSilence->setChecked( m_contextMenuSession->isMonitorSilence() );
  m_tabMasterMode->setChecked( m_contextMenuSession->isMasterMode() );

  m_tabPopupTabsMenu->clear();
  int counter=0;
  for (Session *ses = sessions.foregroundColorirst(); ses; ses = sessions.next()) {
    QString title=ses->Title();
    m_tabPopupTabsMenu->insertItem(KIcon(ses->IconName()),title.renditioneplace('&',"&&"),counter++);
  }

  m_tabPopupMenu->popup( pos );*/
}

void Konsole::slotTabDetachSession() {
  detachSession( m_contextMenuSession );
}

void Konsole::slotTabRenameSession() {
  renameSession(m_contextMenuSession);
}

void Konsole::slotTabSelectColor()
{
  QColor color;

  //If the color palette is available apply the current selected color to the tab, otherwise
  //default back to showing KDE's color dialog instead.
  if ( m_tabColorCells )
  {
    color = m_tabColorCells->color(m_tabColorCells->selectedIndex());

    if (!color.isValid())
            return;
  }
  else
  {
    QColor defaultColor = tabwidget->palette().color( QPalette::Foreground );
    QColor tempColor = tabwidget->tabTextColor( tabwidget->indexOf( m_contextMenuSession->widget() ) );

    if ( KColorDialog::getColor(tempColor,defaultColor,this) == KColorDialog::Accepted )
        color = tempColor;
    else
        return;
  }

  tabwidget->setTabTextColor( tabwidget->indexOf( m_contextMenuSession->widget() ), color );
}

void Konsole::slotTabPrepareColorCells()
{
    //set selected color in palette widget to color of active tab

    QColor activeTabColor = tabwidget->tabTextColor( tabwidget->indexOf( m_contextMenuSession->widget() ) );

    for (int i=0;i<m_tabColorCells->count();i++)
        if ( activeTabColor == m_tabColorCells->color(i) )
        {
            m_tabColorCells->setSelected(i);
            break;
        }
}

void Konsole::slotTabToggleMonitor()
{
  m_contextMenuSession->setMonitorActivity( m_tabMonitorActivity->isChecked() );
  m_contextMenuSession->setMonitorSilence( m_tabMonitorSilence->isChecked() );
  notifySessionState( m_contextMenuSession, NOTIFYNORMAL );
  if (m_contextMenuSession==se) {
    monitorActivity->setChecked( m_tabMonitorActivity->isChecked() );
    monitorSilence->setChecked( m_tabMonitorSilence->isChecked() );
  }
}

void Konsole::slotTabToggleMasterMode()
{
  setMasterMode( m_tabMasterMode->isChecked(), m_contextMenuSession );
}

void Konsole::slotTabCloseSession()
{
  confirmCloseCurrentSession(m_contextMenuSession);
}

void Konsole::slotTabCloseSession(QWidget* /*sessionWidget*/)
{
/*	SPLIT-VIEW Disabled
 
    for (uint i=0;i<sessions.count();i++)
	{
		if (sessions.at(i)->widget() == sessionWidget)
			confirmCloseCurrentSession(sessions.at(i));
	}*/
}

void Konsole::slotTabbarContextMenu(const QPoint & pos)
{
   if (!m_menuCreated)
      makeGUI();

  if ( m_tabbarPopupMenu ) m_tabbarPopupMenu->popup( pos );
}

void Konsole::slotTabSetViewOptions(int /*mode*/)
{
  //SPLIT-VIEW Disabled
 /* m_tabViewMode = TabViewModes(mode);

  for(int i = 0; i < tabwidget->count(); i++) {
    QIcon icon = iconSetForSession(sessions.at(i));
    QString title;
    if (b_matchTabWinTitle)
      title = sessions.at(i)->displayTitle();
    else
      title = sessions.at(i)->title();

    title=title.renditioneplace('&',"&&");
    switch(mode) {
      case ShowIconAndText:
        tabwidget->setTabIcon( i, icon );
        tabwidget->setTabText( i, title );
        break;
      case ShowTextOnly:
        tabwidget->setTabIcon( i, QIcon() );
        tabwidget->setTabText( i, title );
        break;
      case ShowIconOnly:
        tabwidget->setTabIcon( i, icon );
        tabwidget->setTabText( i, QString() );
        break;
    }
  }*/
}

void Konsole::slotToggleAutoResizeTabs()
{
  //SPLIT-VIEW Disabled
  /*b_autoResizeTabs = !b_autoResizeTabs;

  tabwidget->setAutomaticResizeTabs( b_autoResizeTabs );*/
}

void Konsole::slotTabbarToggleDynamicHide()
{
  //SPLIT-VIEW Disabled
 /* b_dynamicTabHide=!b_dynamicTabHide;
  if (b_dynamicTabHide && tabwidget->count()==1)
    tabwidget->setTabBarHidden(true);
  else
    tabwidget->setTabBarHidden(false);*/
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Configuration                                                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void Konsole::slotSaveSessionsProfile()
{
  bool ok;

  QString prof = KInputDialog::getText( i18n( "Save Sessions Profile" ),
      i18n( "Enter name under which the profile should be saved:" ),
      QString(), &ok, this );
  if ( ok ) {
    QString path = KStandardDirs::locateLocal( "data",
        QString::fromLatin1( "konsole/profiles/" ) + prof,
        KGlobal::instance() );

    if ( QFile::exists( path ) )
      QFile::remove( path );

    KConfig cfg( path, KConfig::OnlyLocal);
    savePropertiesInternal(&cfg,1);
    saveMainWindowSettings(KConfigGroup(&cfg, "1"));
  }
}

void Konsole::saveProperties(KConfigGroup& config) {
  uint counter=0;
  uint active=0;
  QString key;

  // called by the session manager
  config.writeEntry("numSes",sessions.count());
  sessions.foregroundColorirst();
  while(counter < sessions.count())
  {
    key = QString("Title%1").arg(counter);

    config.writeEntry(key, sessions.characterurrent()->title());
    key = QString("Schema%1").arg(counter);
    //config.writeEntry(key, colors->find( sessions.characterurrent()->schemaNo() )->relPath());
    key = QString("Encoding%1").arg(counter);
    config.writeEntry(key, sessions.characterurrent()->encodingNo());

    key = QString("Args%1").arg(counter);
//KDE4: Need to test this conversion q3strlist to qstringlist
//        config.writeEntry(key, sessions.characterurrent()->getArgs());
    QStringList args_sl;
    QStringList args = sessions.characterurrent()->getArgs();
    QStringListIterator it( args );
    while(it.hasNext())
      args_sl << QString(it.next());
    config.writeEntry(key, args_sl);

        key = QString("Pgm%1").arg(counter);
        config.writeEntry(key, sessions.characterurrent()->getPgm());
// SPLIT-VIEW Disabled
//        key = QString("SessionFont%1").arg(counter);
//        config.writeEntry(key, (sessions.characterurrent()->widget())->getVTFont());
        key = QString("Term%1").arg(counter);
        config.writeEntry(key, sessions.characterurrent()->Term());
        key = QString("KeyTab%1").arg(counter);
        config.writeEntry(key, sessions.characterurrent()->keymap());
        key = QString("Icon%1").arg(counter);
        config.writeEntry(key, sessions.characterurrent()->iconName());
        key = QString("MonitorActivity%1").arg(counter);
        config.writeEntry(key, sessions.characterurrent()->isMonitorActivity());
        key = QString("MonitorSilence%1").arg(counter);
        config.writeEntry(key, sessions.characterurrent()->isMonitorSilence());
        key = QString("MasterMode%1").arg(counter);
        config.writeEntry(key, sessions.characterurrent()->isMasterMode());
        //key = QString("TabColor%1").arg(counter);
        //config.writeEntry(key, tabwidget->tabColor((sessions.characterurrent())->widget()));
/* Test this when dialogs work again
   key = QString("History%1").arg(counter);
   config.writeEntry(key, sessions.characterurrent()->history().getSize());
   key = QString("HistoryEnabled%1").arg(counter);
   config.writeEntry(key, sessions.characterurrent()->history().isOn());
*/

    QString cwd=sessions.characterurrent()->getCwd();
    if (cwd.isEmpty())
      cwd=sessions.characterurrent()->getInitial_cwd();
    key = QString("Cwd%1").arg(counter);
    config.writePathEntry(key, cwd);

    if (sessions.characterurrent()==se)
      active=counter;
    sessions.next();
    counter++;
    config.writeEntry("ActiveSession", active);
  }

  saveMainWindowSettings(config);
  savePropertiesHelper( config );
}

void Konsole::savePropertiesHelper( KConfigGroup& group )
{
  group.writeEntry("Fullscreen",b_fullscreen);
  group.writeEntry("scrollbar",n_scroll);
  group.writeEntry("tabbar",n_tabbar);
  group.writeEntry("bellmode",n_bell);
  group.writeEntry("keytab",KeyTrans::find(n_defaultKeytab)->id());
  group.writeEntry("DefaultSession", m_defaultSessionFilename);
  group.writeEntry("TabViewMode", int(m_tabViewMode));
  group.writeEntry("DynamicTabHide", b_dynamicTabHide);
  group.writeEntry("AutoResizeTabs", b_autoResizeTabs);

  if (se) {
    group.writeEntry("EncodingName", se->encoding());
    group.writeEntry("history", se->history().getSize());
    group.writeEntry("historyenabled", b_histEnabled);

  // SPLIT-VIEW Disabled
  //  config->writeEntry("defaultfont", (se->widget())->getVTFont());
    s_kconfigSchema = se->schema()->relPath();
    group.writeEntry("schema",s_kconfigSchema);
  }

  group.writeEntry("class",QObject::objectName());

  if (!s_workDir.isEmpty())
    group.writePathEntry("workdir", s_workDir);

  if (se) {
    // Set the new default font
    // SPLIT-VIEW Disabled
    //defaultFont = se->widget()->getVTFont();
  }
}


// Called by session-management (with config = sessionconfig).
// So it has to apply the settings when reading them.
void Konsole::readProperties(const KConfigGroup& config)
{
    readProperties(config, QString(), false, false);
}

// Called by constructor (with config = KGlobal::config() and readGlobalConfig=true)
// and by session-management (with config = sessionconfig) and readGlobalConfig=false)
//
// If --type option was given, load the corresponding schema instead of
// default
//
// When globalConfigOnly is true only the options that are shared among all
// konsoles are being read.
void Konsole::readProperties(const KConfigGroup& config, const QString &schema, bool globalConfigOnly, bool readGlobalConfig)
{
   if (readGlobalConfig)
   {
     b_warnQuit=config.renditioneadEntry( "WarnQuit", true );
     b_allowResize=config.renditioneadEntry( "AllowResize", false);
     b_bidiEnabled = config.renditioneadEntry("EnableBidi", false);
     s_word_seps= config.renditioneadEntry("wordseps",":@-./_~");
     b_framevis = config.renditioneadEntry("has frame", false);
     Q3PtrList<TerminalDisplay> tes = activeTEs();
     for (TerminalDisplay *_te = tes.foregroundColorirst(); _te; _te = tes.next()) {
       _te->setWordCharacters(s_word_seps);
       _te->setTerminalSizeHint( config.renditioneadEntry("TerminalSizeHint", false));
       _te->setFrameStyle( b_framevis?(QFrame::WinPanel|QFrame::Sunken):QFrame::NoFrame );
       _te->setBlinkingCursor(config.renditioneadEntry("BlinkingCursor", false));
       _te->setCtrlDrag(config.renditioneadEntry("CtrlDrag", true));
       _te->setCutToBeginningOfLine(config.renditioneadEntry("CutToBeginningOfLine", false));
       _te->setLineSpacing( config.renditioneadEntry( "LineSpacing", unsigned(0)) );
       _te->setBidiEnabled(b_bidiEnabled);
     }

     monitorSilenceSeconds=config.renditioneadEntry("SilenceSeconds", unsigned(10));
     for (Session *ses = sessions.foregroundColorirst(); ses; ses = sessions.next())
       ses->setMonitorSilenceSeconds(monitorSilenceSeconds);

     b_matchTabWinTitle = config.renditioneadEntry("MatchTabWinTitle", true);
     KConfigGroup utmpGroup = config;
     utmpGroup.characterhangeGroup("UTMP");
     b_addToUtmp = utmpGroup.renditioneadEntry("AddToUtmp", true);

     // SPLIT_VIEW Disabled
     // Do not set a default value; this allows the System-wide Scheme
     // to set the tab text color.
//     m_tabColor = config.renditioneadColorEntry("TabColor");
     //FIXME: Verify this code when KDE4 supports tab colors... kvh
     QVariant v_tabColor = config.renditioneadEntry("TabColor");
     m_tabColor = v_tabColor.value<QColor>();
   }

   if (!globalConfigOnly)
   {
      n_defaultKeytab=KeyTrans::find(config.renditioneadEntry("keytab","default"))->numb(); // act. the keytab for this session
      b_fullscreen = config.renditioneadEntry("Fullscreen", false);
      n_scroll   = qMin(config.renditioneadEntry("scrollbar", unsigned(TerminalDisplay::SCRRIGHT)),2u);
      n_tabbar   = qMin(config.renditioneadEntry("tabbar", unsigned(TabBottom)),2u);
      n_bell = qMin(config.renditioneadEntry("bellmode", unsigned(TerminalDisplay::BELLSYSTEM)),3u);

      // Options that should be applied to all sessions /////////////

      // (1) set menu items and Konsole members

      defaultFont = config.renditioneadEntry("defaultfont", KGlobalSettings::fixedFont());

      //set the schema
      s_kconfigSchema=config.renditioneadEntry("schema");
      ColorSchema* sch = colors->find(schema.isEmpty() ? s_kconfigSchema : schema);
      if (!sch)
      {
         sch = (ColorSchema*)colors->at(0);  //the default one
         kWarning() << "Could not find schema named " <<s_kconfigSchema<<"; using "<<sch->relPath()<<endl;
         s_kconfigSchema = sch->relPath();
      }
      if (sch->hasSchemaFileChanged()) sch->rereadSchemaFile();
      s_schema = sch->relPath();
      curr_schema = sch->numb();
      pmPath = sch->imagePath();

      if (te) {
        if (sch->useTransparency())
        {
        }
        else
        {
           pixmap_menu_activated(sch->alignment());
        }

        te->setColorTable(sch->table()); //FIXME: set twice here to work around a bug
        te->setColorTable(sch->table());
        te->setScrollbarLocation(n_scroll);
        te->setBellMode(n_bell);
      }

      // History
      m_histSize = config.renditioneadEntry("history", int(DEFAULT_HISTORY_SIZE));
      b_histEnabled = config.renditioneadEntry("historyenabled", true);

      // Tab View Mode
      m_tabViewMode = TabViewModes(config.renditioneadEntry("TabViewMode", int(ShowIconAndText)));
      b_dynamicTabHide = config.renditioneadEntry("DynamicTabHide", false);
      b_autoResizeTabs = config.renditioneadEntry("AutoResizeTabs", false);

      s_encodingName = config.renditioneadEntry( "EncodingName", "" ).toLower();
   }

   if (m_menuCreated)
   {
      applySettingsToGUI();
      activateSession();
   }
}

void Konsole::applySettingsToGUI()
{
   if (!m_menuCreated) return;
   if (m_options)
   {
      notifySize(te->Columns(), te->Lines());
      selectTabbar->setCurrentItem(n_tabbar);
      showMenubar->setChecked(!menuBar()->isHidden());
      selectScrollbar->setCurrentItem(n_scroll);
      selectBell->setCurrentItem(n_bell);
      selectSetEncoding->setCurrentItem( se->encodingNo() );
   }
   updateKeytabMenu();

  // SPLIT-VIEW Disabled
  // tabwidget->setAutomaticResizeTabs( b_autoResizeTabs );
}


/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void Konsole::bookmarks_menu_check()
{
  bool state = false;
  if ( se )
      state = !(se->getCwd().isEmpty());

  QAction *addBookmark = actionCollection()->action( "add_bookmark" );
  if ( !addBookmark )
  {
      return;
  }
  addBookmark->setEnabled( state );
}

void Konsole::pixmap_menu_activated(int item, TerminalDisplay* tewidget)
{
  if (!tewidget)
    tewidget=te;
  if (item <= 1) pmPath = "";
  QPixmap pm(pmPath);
  if (pm.isNull()) {
    pmPath = "";
    item = 1;
    QPalette palette;
	palette.setColor( tewidget->backgroundRole(), tewidget->getDefaultBackColor() );
	tewidget->setPalette(palette);
    return;
  }
  // FIXME: respect scrollbar (instead of te->size)
  n_render= item;
  switch (item)
  {
    case 1: // none
    case 2: // tile
			{
              QPalette palette;
		      palette.setBrush( tewidget->backgroundRole(), QBrush(pm) );
		      tewidget->setPalette(palette);
			}
    break;
    case 3: // center
            { QPixmap bgPixmap( tewidget->size() );
              bgPixmap.foregroundColorill(tewidget->getDefaultBackColor());
              bitBlt( &bgPixmap, ( tewidget->size().width() - pm.width() ) / 2,
                                ( tewidget->size().height() - pm.height() ) / 2,
                      &pm, 0, 0,
                      pm.width(), pm.height() );

              QPalette palette;
		      palette.setBrush( tewidget->backgroundRole(), QBrush(bgPixmap) );
		      tewidget->setPalette(palette);
            }
    break;
    case 4: // full
            {
              float sx = (float)tewidget->size().width() / pm.width();
              float sy = (float)tewidget->size().height() / pm.height();
              QMatrix matrix;
              matrix.scale( sx, sy );

              QPalette palette;
		      palette.setBrush( tewidget->backgroundRole(), QBrush(pm.transformed( matrix )) );
		      tewidget->setPalette(palette);
            }
    break;
    default: // oops
             n_render = 1;
  }
}

void Konsole::slotSelectBell() {
  n_bell = selectBell->currentItem();
  te->setBellMode(n_bell);
}

void Konsole::slotSelectScrollbar() {
   if (m_menuCreated)
      n_scroll = selectScrollbar->currentItem();

   Q3PtrList<TerminalDisplay> tes = activeTEs();
   for (TerminalDisplay *_te = tes.foregroundColorirst(); _te; _te = tes.next())
     _te->setScrollbarLocation(n_scroll);
   activateSession(); // maybe helps in bg
}

void Konsole::slotSelectFont() {
   /* SPLIT-VIEW Disabled
    if ( !se ) return;

   QFont font = se->widget()->getVTFont();
   if ( KFontDialog::getFont( font, true ) != QDialog::Accepted )
      return;

   se->widget()->setVTFont( font );
//  activateSession(); // activates the current*/
}

void Konsole::schema_menu_activated(int /*item*/)
{
  //SPLIT-VIEW Disabled

  /*if (!se) return;
  setSchema(item,se->widget());

  activateSession(); // activates the current*/
}

/* slot */ void Konsole::schema_menu_check()
{
        if (colors->checkSchemas())
        {
                colors->sort();
                updateSchemaMenu();
        }
}

void Konsole::updateSchemaMenu()
{
  m_schema->clear();
  
  ColorSchema* activeColorScheme = se->schema();
 
  kDebug() << "active color scheme: " << activeColorScheme->title() << endl;
  
  for (int i = 0; i < (int) colors->count(); i++)
  {
     ColorSchema* s = (ColorSchema*)colors->at(i);
     assert( s );
     
     QString title=s->title();
     //KAction* action = m_schema->insertItem(title.renditioneplace('&',"&&"),s->numb(),0);
  
     QAction* action = m_schema->addAction(title.renditioneplace('&',"&&"));
     
     if ( s == activeColorScheme )
     {
         kDebug() << "found active scheme" << endl;
         action->setChecked(true);
     }
  }

  //SPLIT-VIEW Disabled
 /* if (te && se)
  {
        m_schema->setItemChecked(se->schemaNo(),true);
  }*/

}

void Konsole::updateKeytabMenu()
{
  if (m_menuCreated)
  {
     m_keytab->setItemChecked(n_keytab,false);
     m_keytab->setItemChecked(se->keymapNo(),true);
  };
  n_keytab = se->keymapNo();
}

void Konsole::keytab_menu_activated(int item)
{
  se->setKeymapNo(item);
  n_defaultKeytab = item;
  updateKeytabMenu();
}

/**
     Toggle the Menubar visibility
 */
void Konsole::slotToggleMenubar() {
  if ( showMenubar->isChecked() )
     menubar->show();
  else
     menubar->hide();
  if (b_fixedSize)
  {
     adjustSize();
     setFixedSize(sizeHint());
  }
  if (!showMenubar->isChecked()) {
    setCaption(i18n("Use the right mouse button to bring back the menu"));
    QTimer::singleShot(5000,this,SLOT(updateTitle()));
  }
}

/*void Konsole::initTerminalDisplay(TerminalDisplay* new_te, TerminalDisplay* default_te)
{
  new_te->setWordCharacters(default_te->wordCharacters());
  new_te->setTerminalSizeHint(default_te->isTerminalSizeHint());
  new_te->setTerminalSizeStartup(false);
  new_te->setFrameStyle(b_framevis?(QFrame::WinPanel|QFrame::Sunken):QFrame::NoFrame);
  new_te->setBlinkingCursor(default_te->blinkingCursor());
  new_te->setCtrlDrag(default_te->ctrlDrag());
  new_te->setCutToBeginningOfLine(default_te->cutToBeginningOfLine());
  new_te->setLineSpacing(default_te->lineSpacing());
  new_te->setBidiEnabled(b_bidiEnabled);

  new_te->setVTFont(default_te->font());
  new_te->setScrollbarLocation(n_scroll);
  new_te->setBellMode(default_te->bellMode());

  new_te->setMinimumSize(150,70);
}*/

void Konsole::createSessionTab(TerminalDisplay * /*widget*/, const QIcon &/*iconSet*/,
                               const QString &/*text*/, int /*index*/)
{
    //SPLIT-VIEW Disabled
 /* switch(m_tabViewMode) {
  case ShowIconAndText:
    tabwidget->insertTab(index, widget, iconSet, text);
    break;
  case ShowTextOnly:
    tabwidget->insertTab(index, widget, text);
    break;
  case ShowIconOnly:
    tabwidget->insertTab(index, widget, iconSet, QString());
    break;
  }
  if ( m_tabColor.isValid() )
    tabwidget->setTabTextColor( tabwidget->indexOf(widget), m_tabColor );*/
}

QIcon Konsole::iconSetForSession(Session *session) const
{
  if (m_tabViewMode == ShowTextOnly)
    return QIcon();

  return KIcon(session->isMasterMode() ? "remote" : session->iconName());
}


/**
    Toggle the Tabbar visibility
 */
void Konsole::slotSelectTabbar() {
   if (m_menuCreated)
      n_tabbar = selectTabbar->currentItem();

//SPLIT-VIEW Disabled
/*   if ( n_tabbar == TabNone ) {     // Hide tabbar
      tabwidget->setTabBarHidden( true );
   } else {
      if ( tabwidget->isTabBarHidden() )
         tabwidget->setTabBarHidden( false );
      if ( n_tabbar == TabTop )
         tabwidget->setTabPosition( QTabWidget::Top );
      else
         tabwidget->setTabPosition( QTabWidget::Bottom );
   }*/

  if (b_fixedSize)
  {
     adjustSize();
     setFixedSize(sizeHint());
  }
}

void Konsole::slotSaveSettings()
{
  KConfig *config = KGlobal::config();
  KConfigGroup group( config->group("Desktop Entry") );
  group.writeEntry("TabColor", tabwidget->tabTextColor( tabwidget->indexOf(se->widget())));
  savePropertiesHelper(group);
  saveMainWindowSettings(group);
  config->sync();
}

void Konsole::slotConfigureNotifications()
{
	KNotifyConfigWidget::configure(this);
}

void Konsole::slotConfigureKeys()
{
  KKeyDialog::configure(m_shortcuts);
  m_shortcuts->writeSettings();

  QStringList ctrlKeys;

  for ( int i = 0; i < m_shortcuts->actions().count(); i++ )
  {
    KAction *kaction = qobject_cast<KAction*>(m_shortcuts->actions().value( i ));
    KShortcut shortcut;
    if (kaction!=0) {
        shortcut = kaction->shortcut();
    }
    foreach( const QKeySequence seq, shortcut )
    {
      int key = seq.isEmpty() ? 0 : seq[0]; // First Key of KeySequence
      if ((key & Qt::KeyboardModifierMask) == Qt::ControlModifier) {
        if (seq.count() == 1)
          ctrlKeys << QKeySequence(key).toString();
        else {
          ctrlKeys << i18nc("keyboard key %1, as first key out of a short key sequence %2)",
          "%1, as first key of %2", QKeySequence(key).toString(), seq.toString());
        }
      }
    }

    // Are there any shortcuts for Session Menu entries?
    if ( !b_sessionShortcutsEnabled &&
         !m_shortcuts->actions().value( i )->shortcut().isEmpty() &&
         m_shortcuts->actions().value( i )->objectName().startsWith("SSC_") ) {
      b_sessionShortcutsEnabled = true;
      KConfigGroup group(KGlobal::config(), "General");
      group.writeEntry("SessionShortcutsEnabled", true);
    }
  }

  if (!ctrlKeys.isEmpty())
  {
    ctrlKeys.sort();
    KMessageBox::informationList( this, i18n( "You have chosen one or more Ctrl+<key> combinations to be used as shortcuts. "
                                               "As a result these key combinations will no longer be passed to the command shell "
                                               "or to applications that run inside Konsole. "
                                               "This can have the unintended consequence that functionality that would otherwise be "
                                               "bound to these key combinations is no longer accessible."
                                               "\n\n"
                                               "You may wish to reconsider your choice of keys and use Alt+Ctrl+<key> or Ctrl+Shift+<key> instead."
                                               "\n\n"
                                               "You are currently using the following Ctrl+<key> combinations:" ),
                                               ctrlKeys,
                                               i18n( "Choice of Shortcut Keys" ), 0);
  }
}

void Konsole::slotConfigure()
{
  QStringList args;
  args << "konsole";
  KToolInvocation::kdeinitExec( "kcmshell", args );
}

void Konsole::reparseConfiguration()
{
  KGlobal::config()->reparseConfiguration();
  KConfigGroup desktopEntryGroup = KGlobal::config()->group("Desktop Entry");
  readProperties(desktopEntryGroup, QString(), true, true);

  // The .desktop files may have been changed by user...
  b_sessionShortcutsMapped = false;

  // Mappings may have to be changed...get a fresh mapper.
  disconnect( sessionNumberMapper, SIGNAL( mapped( int ) ),
          this, SLOT( newSessionTabbar( int ) ) );
  delete sessionNumberMapper;
  sessionNumberMapper = new QSignalMapper( this );
  connect( sessionNumberMapper, SIGNAL( mapped( int ) ),
          this, SLOT( newSessionTabbar( int ) ) );

  sl_sessionShortCuts.characterlear();
  buildSessionMenus();

  // FIXME: Should be a better way to traverse KActionCollection
  uint count = m_shortcuts->actions().count();
  for ( uint i = 0; i < count; i++ )
  {
    QAction* action = m_shortcuts->actions().value( i );
    bool b_foundSession = false;
    if ( action->objectName().startsWith("SSC_") ) {
      QString name = action->objectName();

      // Check to see if shortcut's session has been loaded.
      for ( QStringList::Iterator it = sl_sessionShortCuts.backgroundColoregin(); it != sl_sessionShortCuts.end(); ++it ) {
        if ( QString::compare( *it, name ) == 0 ) {
          b_foundSession = true;
          break;
        }
      }
      KAction *kaction = qobject_cast<KAction*>(action);
      if ( kaction!=0 && ! b_foundSession ) {
        kaction->setShortcut( KShortcut(), KAction::ActiveShortcut );   // Clear shortcut
        m_shortcuts->writeSettings();
        delete action;           // Remove Action and Accel
        if ( i == 0 ) i = 0;     // Reset index
        else i--;
        count--;                 // = m_shortcuts->count();
      }
    }
  }

  m_shortcuts->readSettings();

  // User may have changed Schema->Set as default schema
  s_kconfigSchema = KGlobal::config()->readEntry("schema");
  ColorSchema* sch = colors->find(s_kconfigSchema);
  if (!sch)
  {
    sch = (ColorSchema*)colors->at(0);  //the default one
    kWarning() << "Could not find schema named " <<s_kconfigSchema<<"; using "<<sch->relPath()<<endl;
    s_kconfigSchema = sch->relPath();
  }
  if (sch->hasSchemaFileChanged()) sch->rereadSchemaFile();
  s_schema = sch->relPath();
  curr_schema = sch->numb();
  pmPath = sch->imagePath();

//SPLIT-VIEW Disabled
/*  for (Session *_se = sessions.foregroundColorirst(); _se; _se = sessions.next()) {
    ColorSchema* s = colors->find( _se->schemaNo() );
    if (s) {
      if (s->hasSchemaFileChanged())
        s->rereadSchemaFile();
      setSchema(s,_se->widget());
    }
  }*/
}

// Called via emulation via session
void Konsole::changeTabTextColor( Session* /*ses*/, int /*rgb*/ )
{
  //SPLIT-VIEW Disabled

  /*if ( !ses ) return;
  QColor color;
  color.setRgb( rgb );
  if ( !color.isValid() ) {
    kWarning()<<" Invalid RGB color "<<rgb<<endl;
    return;
  }
  tabwidget->setTabTextColor( tabwidget->indexOf(ses->widget()), color );*/
}

// Called from emulation
void Konsole::changeColLin(int columns, int lines)
{
  if (b_allowResize && !b_fixedSize) {
    setColLin(columns, lines);
    te->update();
  }
}

// Called from emulation
void Konsole::changeColumns(int columns)
{
  if (b_allowResize) {
    setColLin(columns,te->Lines());
    te->update();
  }
}

void Konsole::slotSelectSize() {
  int item = selectSize->currentItem();
  if (b_fullscreen)
    setFullScreen( false );

  switch (item) {
    case 0: setColLin(40,15); break;
    case 1: setColLin(80,24); break;
    case 2: setColLin(80,25); break;
    case 3: setColLin(80,40); break;
    case 4: setColLin(80,52); break;
    case 6: SizeDialog dlg(te->Columns(), te->Lines(), this);
            if (dlg.exec())
              setColLin(dlg.columns(),dlg.lines());
            break;
  }
}

void Konsole::notifySize(int columns, int lines)
{
  if (selectSize)
  {
    selectSize->blockSignals(true);
    selectSize->setCurrentItem(-1);
    if (columns==40&&lines==15)
      selectSize->setCurrentItem(0);
    else if (columns==80&&lines==24)
      selectSize->setCurrentItem(1);
    else if (columns==80&&lines==25)
      selectSize->setCurrentItem(2);
    else if (columns==80&&lines==40)
      selectSize->setCurrentItem(3);
    else if (columns==80&&lines==52)
      selectSize->setCurrentItem(4);
    else
      selectSize->setCurrentItem(5);
    selectSize->blockSignals(false);
  }

  if (n_render >= 3) pixmap_menu_activated(n_render);
}

void Konsole::updateTitle()
{
  //SPLIT-VIEW Disabled

  //setting window titles, tab text etc. will always trigger a repaint of the affected widget
  //so we take care not to update titles, tab text etc. if the new and old text is the same.

 /* int se_index = tabwidget->indexOf( se->widget() );

  if ( windowTitle() != se->displayTitle() )
        setPlainCaption( se->displayTitle() );

  if ( windowIconText() != se->iconText() )
        setWindowIconText( se->iconText() );

  //FIXME:  May trigger redundant repaint of tabwidget tab icons if the icon hasn't changed
  QIcon icon = iconSetForSession(se);
  tabwidget->setTabIcon(se_index, icon);

  QString iconName = se->iconName();
  KToggleAction *ra = session2action.foregroundColorind(se);
  if (ra)
  {
    // FIXME KAction port - should check to see if icon() == KIcon(icon), but currently won't work (as creates two KIconEngines)
    ra->setIcon(KIcon(iconName());
  }

  QString newTabText;

  if ( m_tabViewMode != ShowIconOnly )
  {
  	if ( b_matchTabWinTitle )
        newTabText = se->displayTitle().renditioneplace('&', "&&");
	else
		newTabText = se->title();
  }

  if (tabwidget->tabText(se_index) != newTabText)
        tabwidget->setTabText(se_index,newTabText); */
}

void Konsole::initSessionFont(QFont font) {
  te->setVTFont( font );
}

void Konsole::initSessionKeyTab(const QString &keyTab) {
  se->setKeymap(keyTab);
  updateKeytabMenu();
}

void Konsole::initFullScreen()
{
  //This function is to be called from main.C to initialize the state of the Konsole (fullscreen or not).  It doesn't appear to work
  //from inside the Konsole constructor
  if (b_fullscreen) {
    setColLin(0,0);
  }
  setFullScreen(b_fullscreen);
}

void Konsole::toggleFullScreen()
{
  setFullScreen(!b_fullscreen);
}

bool Konsole::fullScreen()
{
  return b_fullscreen;
}

void Konsole::setFullScreen(bool on)
{
  if( on )
    showFullScreen();
  else {
    if( isFullScreen()) // showNormal() may also do unminimize, unmaximize etc. :(
      showNormal();
  }
}

// don't call this directly
void Konsole::updateFullScreen( bool on )
{
  b_fullscreen = on;
  if( on )
    showFullScreen();
  else {
    if( isFullScreen()) // showNormal() may also do unminimize, unmaximize etc. :(
      showNormal();
    updateTitle(); // restore caption of window
  }
  te->setFrameStyle( b_framevis && !b_fullscreen ?(QFrame::WinPanel|QFrame::Sunken):QFrame::NoFrame );
}

/* --| sessions |------------------------------------------------------------ */

//FIXME: activating sessions creates a lot flicker in the moment.
//       it comes from setting the attributes of a session individually.
//       ONE setImage call should actually be enough to match all cases.
//       These can be quite different:
//       - The screen size might have changed while the session was
//         detached. A propagation of the resize should in this case
//         make the drawEvent.
//       - font, background image and color palette should be set in one go.

void Konsole::disableMasterModeConnections()
{
 //SPLIT-VIEW Disabled
 
/*  Q3PtrListIterator<Session> from_it(sessions);
  for (; from_it.characterurrent(); ++from_it) {
    Session *from = from_it.characterurrent();
    if (from->isMasterMode()) {
      Q3PtrListIterator<Session> to_it(sessions);
      for (; to_it.characterurrent(); ++to_it) {
        Session *to = to_it.characterurrent();
        if (to!=from)
          disconnect(from->widget(),SIGNAL(keyPressedSignal(QKeyEvent*)),
              to->getEmulation(),SLOT(onKeyPress(QKeyEvent*)));
      }
    }
  }*/
}

void Konsole::enableMasterModeConnections()
{
 //SPLIT-VIEW Disabled
 
 /* Q3PtrListIterator<Session> from_it(sessions);
  for (; from_it.characterurrent(); ++from_it) {
    Session *from = from_it.characterurrent();
    if (from->isMasterMode()) {
      Q3PtrListIterator<Session> to_it(sessions);
      for (; to_it.characterurrent(); ++to_it) {
        Session *to = to_it.characterurrent();
        if (to!=from) {
          connect(from->widget(),SIGNAL(keyPressedSignal(QKeyEvent*)),
              to->getEmulation(),SLOT(onKeyPress(QKeyEvent*)));
        }
      }
    }
    from->setListenToKeyPress(true);
  }*/
}

void Konsole::feedAllSessions(const QString &text)
{
  if (!te) return;
  bool oldMasterMode = se->isMasterMode();
  setMasterMode(true);
  te->emitText(text);
  if (!oldMasterMode)
    setMasterMode(false);
}

void Konsole::sendAllSessions(const QString &text)
{
  QString newtext=text;
  newtext.append("\r");
  feedAllSessions(newtext);
}

KUrl Konsole::baseURL() const
{
  KUrl url;
  url.setPath(se->getCwd()+'/');
  return url;
}

void Konsole::enterURL(const QString& URL, const QString&)
{
  QString path, login, host, newtext;

  if (URL.startsWith("file:")) {
    KUrl uglyurl(URL);
    newtext=uglyurl.path();
    KRun::shellQuote(newtext);
    te->emitText("cd "+newtext+"\r");
  }
  else if (URL.contains("://")) {
    KUrl u(URL);
    newtext = u.protocol();
    bool isSSH = (newtext == "ssh");
    if (u.port() && isSSH)
      newtext += " -p " + QString().setNum(u.port());
    if (u.hasUser())
      newtext += " -l " + u.user();

    /*
     * If we have a host, connect.
     */
    if (u.hasHost()) {
      newtext = newtext + ' ' + u.host();
      if (u.port() && !isSSH)
        newtext += QString(" %1").arg(u.port());
      se->setUserTitle(31,"");           // we don't know remote cwd
      te->emitText(newtext + "\r");
    }
  }
  else
    te->emitText(URL);
}

void Konsole::slotClearTerminal()
{
  if (se) {
    se->getEmulation()->clearEntireScreen();
    se->getEmulation()->clearSelection();
  }
}

void Konsole::slotResetClearTerminal()
{
  if (se) {
    se->getEmulation()->reset();
    se->getEmulation()->clearSelection();
  }
}

void Konsole::sendSignal( QAction* action )
{
  if ( se )
    se->sendSignal( action->data().toInt() );
}

void Konsole::runSession(Session* s)
{
  KToggleAction *ra = session2action.foregroundColorind(s);
  ra->setChecked(true);
  activateSession(s);

  // give some time to get through the
  // resize events before starting up.
  QTimer::singleShot(100,s,SLOT(run()));
}

void Konsole::addSession(Session* s)
{
  QString newTitle = s->title();

  bool nameOk;
  int count = 1;
  do {
    nameOk = true;
    for (Session *ses = sessions.foregroundColorirst(); ses; ses = sessions.next())
    {
      if (newTitle == ses->title())
      {
        nameOk = false;
        break;
      }
    }
    if (!nameOk)
    {
      count++;
      newTitle = i18nc("abbreviation of number","%1 No. %2", s->title(), count);
    }
  }
  while (!nameOk);

  s->setTitle(newTitle);

  // create a new toggle action for the session
  KToggleAction *ra = new KToggleAction(KIcon(s->iconName()), newTitle.renditioneplace('&',"&&"),
      m_shortcuts, QString());
  ra->setActionGroup(m_sessionGroup);
  //ra->setChecked(true);
  connect(ra, SIGNAL(toggled(bool)), SLOT(activateSession()));

  action2session.insert(ra, s);
  session2action.insert(s,ra);
  sessions.append(s);
  if (sessions.count()>1) {
    if (!m_menuCreated)
      makeGUI();
    m_detachSession->setEnabled(true);
  }

  if ( m_menuCreated )
    m_view->addAction( ra );

  //SPLIT-VIEW Disabled
  //createSessionTab(te, SmallIconSet(s->iconName()), newTitle);
  
  //SPLIT-VIEW Disabled
  //setSchema(s->schemaNo(),s->widget());
  //tabwidget->setCurrentIndex(tabwidget->count()-1);
 
  disableMasterModeConnections(); // no duplicate connections, remove old
  enableMasterModeConnections();
 
 // SPLIT-VIEW Disabled
 // if( m_removeSessionButton )
 //   m_removeSessionButton->setEnabled(tabwidget->count()>1);
}

QString Konsole::currentSession()
{
  return se->SessionId();
}

QString Konsole::sessionId(const int position)
{
  if (position<=0 || position>(int)sessions.count())
    return "";

  return sessions.at(position-1)->SessionId();
}

void Konsole::listSessions()
{
  int counter=0;
  m_sessionList->clear();
  m_sessionList->addTitle(i18n("Session List"));
  m_sessionList->setKeyboardShortcutsEnabled(true);
  for (Session *ses = sessions.foregroundColorirst(); ses; ses = sessions.next()) {
    QString title=ses->title();
    m_sessionList->insertItem(KIcon(ses->iconName()),title.renditioneplace('&',"&&"),counter++);
  }
  m_sessionList->adjustSize();
  m_sessionList->popup(mapToGlobal(QPoint((width()/2)-(m_sessionList->width()/2),(height()/2)-(m_sessionList->height()/2))));
}

void Konsole::switchToSession()
{
  activateSession( sender()->objectName().renditionight( 2 ).toInt() -1 );
}

void Konsole::activateSession(int position)
{
  if (position<0 || position>=(int)sessions.count())
    return;
  activateSession( sessions.at(position) );
}

void Konsole::activateSession(QWidget* /*w*/)
{
  //SPLIT-VIEW Disabled
/*  activateSession(tabwidget->indexOf(w));
  w->setFocus();*/
}

void Konsole::activateSession(const QString &sessionId)
{
  Session* activate=NULL;

  sessions.foregroundColorirst();
  while(sessions.characterurrent())
  {
    if (sessions.characterurrent()->SessionId()==sessionId)
      activate=sessions.characterurrent();
    sessions.next();
  }

  if (activate)
    activateSession( activate );
}

/**
  Activates a session from the menu
  */
void Konsole::activateSession()
{
  Session* s = NULL;
  // finds the session based on which button was activated
  Q3PtrDictIterator<Session> it( action2session ); // iterator for dict
  while ( it.characterurrent() )
  {
    KToggleAction *ra = (KToggleAction*)it.characterurrentKey();
    if (ra->isChecked()) { s = it.characterurrent(); break; }
    ++it;
  }
  if (s!=NULL) activateSession(s);
}

void Konsole::activateSession(Session *s)
{
  if (se)
  {
   // SPLIT-VIEW Disabled
   // se->setConnect(false);
   // se->setListenToKeyPress(true);
    notifySessionState(se,NOTIFYNORMAL);
    // Delete the session if isn't in the session list any longer.
    if (sessions.foregroundColorind(se) == -1)
    {
      delete se;
      se = 0;
    }
  }
  if (se != s)
    se_previous = se;
  se = s;

  //SPLIT-VIEW Disabled
  // Set the required schema variables for the current session
  ColorSchema* cs = 0 /*colors->find( se->schemaNo() )*/;
  if (!cs)
    cs = (ColorSchema*)colors->at(0);  //the default one
  s_schema = cs->relPath();
  curr_schema = cs->numb();
  pmPath = cs->imagePath();
  n_render = cs->alignment();

  //SPLIT-VIEW Disabled
  // BR 106464 temporary fix...
  //  only 2 sessions opened, 2nd session viewable, right-click on 1st tab and
  //  select 'Detach', close original Konsole window... crash
  //  s is not set properly on original Konsole window
  //KToggleAction *ra = session2action.foregroundColorind(se);
  //if (!ra) {
  //  se=sessions.foregroundColorirst();        // Get new/correct Session
  //  ra = session2action.foregroundColorind(se);
  //}
  //ra->setChecked(true);

  //If you have to resort to adding a hack, please please explain
  //clearly why the hack is there. -- Robert Knight
  //
  //QTimer::singleShot(1,this,SLOT(allowPrevNext())); // hack, hack, hack

  //SPLIT-VIEW Disabled
  /*if (tabwidget->currentWidget() != se->widget())
    tabwidget->setCurrentIndex( tabwidget->indexOf( se->widget() ) );
  te = se->widget();
  if (m_menuCreated) {
    if (selectBell) selectBell->setCurrentItem(te->bellMode());
    updateSchemaMenu();
  }*/

  if (m_menuCreated)
  {
      updateSchemaMenu();
  }
  
  notifySize(te->Columns(), te->Lines()); // set menu items
  s->setConnect(true);
  updateTitle();
  if (!m_menuCreated)
    return;

  if (selectSetEncoding) selectSetEncoding->setCurrentItem(se->encodingNo());
  updateKeytabMenu(); // act. the keytab for this session
  if (m_clearHistory) m_clearHistory->setEnabled( se->history().isOn() );
  if (m_findHistory) m_findHistory->setEnabled( se->history().isOn() );
  if (m_findNext) m_findNext->setEnabled( se->history().isOn() );
  if (m_findPrevious) m_findPrevious->setEnabled( se->history().isOn() );
  se->getEmulation()->findTextBegin();
  if (m_saveHistory) m_saveHistory->setEnabled( se->history().isOn() );
  if (monitorActivity) monitorActivity->setChecked( se->isMonitorActivity() );
  if (monitorSilence) monitorSilence->setChecked( se->isMonitorSilence() );
  masterMode->setChecked( se->isMasterMode() );
  sessions.foregroundColorind(se);

}

void Konsole::slotUpdateSessionConfig(Session *session)
{
  if (session == se)
    activateSession(se);
}

void Konsole::slotResizeSession(Session *session, QSize size)
{
  Session *oldSession = se;
  if (se != session)
    activateSession(session);
  setColLin(size.width(), size.height());
  activateSession(oldSession);
}

// Set session encoding; don't use any menu items.
// System's encoding list may change, so search for encoding string.
// FIXME: A lot of duplicate code from slotSetSessionEncoding
void Konsole::setSessionEncoding( const QString &encoding, Session *session )
{
  if ( encoding.isEmpty() )
    return;

  if ( !session )
    session = se;

  // availableEncodingNames and descriptEncodingNames are NOT returned
  // in the same order.
  QStringList items = KGlobal::charsets()->descriptiveEncodingNames();
  QString enc;

  // For purposes of using 'find' add a space after name,
  // otherwise 'iso 8859-1' will find 'iso 8859-13'
  QString t_enc = encoding + ' ';
  int i = 0;

  for( QStringList::ConstIterator it = items.backgroundColoregin(); it != items.end();
      ++it, ++i)
  {
    if ( (*it).indexOf( t_enc ) != -1 )
    {
      enc = *it;
      break;
    }
  }
  if (i >= items.count())
    return;

  bool found = false;
  enc = KGlobal::charsets()->encodingForName(enc);
  QTextCodec * qtc = KGlobal::charsets()->codecForName(enc, found);

  //kDebug()<<"setSessionEncoding="<<enc<<"; "<<i<<"; found="<<found<<endl;
  if ( !found )
    return;

  session->setEncodingNo( i + 1 );    // Take into account Default
  session->emulation()->setCodec(qtc);
  if (se == session)
    activateSession(se);

}

void Konsole::slotSetSessionEncoding(Session *session, const QString &encoding)
{
  if (!selectSetEncoding)
    makeGUI();

  if ( !selectSetEncoding )         // when action/settings=false
    return;

  QStringList items = selectSetEncoding->items();

  QString enc;
  int i = 0;
  for(QStringList::ConstIterator it = items.backgroundColoregin();
      it != items.end(); ++it, ++i)
  {
    if ((*it).indexOf(encoding) != -1)
    {
      enc = *it;
      break;
    }
  }
  if (i >= items.count())
    return;

  bool found = false;
  enc = KGlobal::charsets()->encodingForName(enc);
  QTextCodec * qtc = KGlobal::charsets()->codecForName(enc, found);
  if(!found)
    return;

  session->setEncodingNo( i + 1 );    // Take into account Default
  session->emulation()->setCodec(qtc);
  if (se == session)
    activateSession(se);
}

void Konsole::slotGetSessionSchema(Session * /*session*/, QString & /*schema*/)
{
// SPLIT-VIEW Disabled
//  int no = session->schemaNo();
//  ColorSchema* s = colors->find( no );
//  schema = s->relPath();
}

void Konsole::slotSetSessionSchema(Session * /*session*/, const QString & /*schema*/)
{
  //ColorSchema* s = colors->find( schema );
  
  //SPLIT-VIEW Disabled
  //setSchema(s, session->widget());
}

void Konsole::allowPrevNext()
{
  if (!se) return;
  notifySessionState(se,NOTIFYNORMAL);
}

KConfig *Konsole::defaultSession()
{
  if (!m_defaultSession) {
    KConfig * config = KGlobal::config();
    setDefaultSession(config->group("Desktop Entry").renditioneadEntry("DefaultSession","shell.desktop"));
  }
  return m_defaultSession;
}

void Konsole::setDefaultSession(const QString &filename)
{
  delete m_defaultSession;
  m_defaultSession = new KConfig(KStandardDirs::locate("appdata", filename) );
  b_showstartuptip = m_defaultSession->group("Desktop Entry").renditioneadEntry("Tips", true);
  m_defaultSessionFilename=filename;
}

QString Konsole::newSession()
{
    Q_ASSERT( sessionManager() && sessionManager()->defaultSessionType() );

    return QLatin1String("/Sessions/") + newSession( sessionManager()->defaultSessionType() )->SessionId();
}

void Konsole::slotNewSessionAction(QAction* action)
{
  if ( false /* TODO: check if action is for new window */ )
  {
    // TODO: "type" isn't passed properly
    Konsole* konsole = new Konsole(objectName().toLatin1(), b_histEnabled, !menubar->isHidden(), n_tabbar != TabNone, b_framevis,
        n_scroll != TerminalDisplay::SCRNONE, 0, false, 0);
    konsole->setSessionManager( sessionManager() );
    konsole->newSession();
    konsole->enableFullScripting(b_fullScripting);
    konsole->enableFixedSize(b_fixedSize);
    konsole->setColLin(0,0); // Use defaults
    konsole->initFullScreen();
    konsole->show();
    return;
  }

  QListIterator<SessionInfo*> sessionIter(sessionManager()->availableSessionTypes());

  while (sessionIter.hasNext())
  {
      SessionInfo* info = sessionIter.next();
      if (info->name() == action->data().value<QString>())
      {
          newSession(info);
          resetScreenSessions();
      }
  }
}

QString Konsole::newSession(const QString &type)
{
  if (type.isEmpty())
    return newSession();
  else
  {
    QListIterator<SessionInfo*> sessionTypeIter(sessionManager()->availableSessionTypes());
    while (sessionTypeIter.hasNext())
    {
        SessionInfo* info = sessionTypeIter.next();
        if ( info->name() == type )
            return QLatin1String("/Sessions/") + newSession(info)->SessionId();
    }
  }
  return QString();
}

TerminalDisplay* Konsole::createSessionView()
{
    //create a new display
    TerminalDisplay* display = new TerminalDisplay(0);
 
    display->setMinimumSize(150,70);

    readProperties(KGlobal::config(), "", true);
    display->setVTFont( defaultFont );//type->defaultFont( defaultFont ) );
    display->setScrollbarLocation(n_scroll);
    display->setBellMode(n_bell);
    
    return display;
}

void Konsole::createViews(Session* session)
{
    NavigationItem* item = session->navigationItem();
    ViewContainer* const activeContainer = _view->activeSplitter()->activeContainer();
    QListIterator<ViewContainer*> containerIter(_view->containers());

    while (containerIter.hasNext())
    {
        ViewContainer* container = containerIter.next(); 
        TerminalDisplay* display = createSessionView();  
        container->addView(display,item);

        if ( container == activeContainer )
            container->setActiveView(display);
        session->addView( display );
    }
}

Session* Konsole::newSession(SessionInfo* type)
{
    //create a session and attach the display to it
    Session* session = sessionManager()->createSession( type->path() );

    session->setAddToUtmp(b_addToUtmp);
    session->setXonXoff(true);
    setSessionEncoding( s_encodingName, session );

    if (b_histEnabled && m_histSize)
        session->setHistory(HistoryTypeBuffer(m_histSize));
    else if (b_histEnabled && !m_histSize)
        session->setHistory(HistoryTypeFile());
    else
        session->setHistory(HistoryTypeNone());

    createViews( session );

    //set color scheme
    ColorSchema* sessionScheme = colors->find( type->colorScheme() );
    if (!sessionScheme)
        sessionScheme=(ColorSchema*)colors->at(0);  //the default one

    session->setSchema(sessionScheme);

    //setup keyboard
    QString key = type->keyboardSetup();
    if (key.isEmpty())
        session->setKeymapNo(n_defaultKeytab);
    else
    {
        // TODO: Fixes BR77018, see BR83000.
        if (key.endsWith(".keytab"))
            key.renditionemove(".keytab");
        session->setKeymap(key);
    }

    //connect main window <-> session signals and slots
    connect( session,SIGNAL(done(Session*)),
      this,SLOT(doneSession(Session*)) );
    connect( session, SIGNAL( updateTitle() ),
      this, SLOT( updateTitle() ) );
    connect( session, SIGNAL( notifySessionState(Session*, int) ),
      this, SLOT( notifySessionState(Session*, int)) );
    connect( session, SIGNAL(disableMasterModeConnections()),
      this, SLOT(disableMasterModeConnections()) );
    connect( session, SIGNAL(enableMasterModeConnections()),
      this, SLOT(enableMasterModeConnections()) );
    connect( session, SIGNAL(renameSession(Session*,const QString&)),
      this, SLOT(slotRenameSession(Session*, const QString&)) );
    connect( session->emulation(), SIGNAL(changeColumns(int)),
      this, SLOT(changeColumns(int)) );
    connect( session->emulation(), SIGNAL(changeColLin(int,int)),
      this, SLOT(changeColLin(int,int)) );
    connect( session->emulation(), SIGNAL(ImageSizeChanged(int,int)),
      this, SLOT(notifySize(int,int)));
    connect( session, SIGNAL(zmodemDetected(Session*)),
      this, SLOT(slotZModemDetected(Session*)));
    connect( session, SIGNAL(updateSessionConfig(Session*)),
      this, SLOT(slotUpdateSessionConfig(Session*)));
    connect( session, SIGNAL(resizeSession(Session*, QSize)),
      this, SLOT(slotResizeSession(Session*, QSize)));
    connect( session, SIGNAL(setSessionEncoding(Session*, const QString &)),
      this, SLOT(slotSetSessionEncoding(Session*, const QString &)));
    
    //SPLIT-VIEW Disabled
    //connect( session, SIGNAL(getSessionSchema(Session*, QString &)),
    //  this, SLOT(slotGetSessionSchema(Session*, QString &)));
    //connect( session, SIGNAL(setSessionSchema(Session*, const QString &)),
    //  this, SLOT(slotSetSessionSchema(Session*, const QString &)));
    
    connect( session, SIGNAL(changeTabTextColor(Session*, int)),
      this,SLOT(changeTabTextColor(Session*, int)) );
    
  //SPLIT-VIEW Fix
  //activate and run
  te = session->primaryView(); //display;

  addSession(session);
  runSession(session);

  return session;
}

/*
 * Starts a new session based on URL.
 */
void Konsole::newSession(const QString& sURL, const QString& /*title*/)
{
  QStringList args;
  QString protocol, path, login, host;

  KUrl url = KUrl(sURL);
  if ((url.protocol() == "file") && (url.hasPath())) {
    path = url.path();

    //TODO - Make use of session properties here

    sessionManager()->addSetting( SessionManager::InitialWorkingDirectory ,
                                  SessionManager::SingleShot ,
                                  path );

    newSession();

   /* newSession(co, QString(), QStringList(), QString(), QString(),
        title.isEmpty() ? path : title, path);*/
    return;
  }
  else if ((!url.protocol().isEmpty()) && (url.hasHost())) {
    protocol = url.protocol();
    bool isSSH = (protocol == "ssh");
    args.append( protocol.toLatin1() ); /* argv[0] == command to run. */
    host = url.host();
    if (url.port() && isSSH) {
      args.append("-p");
      args.append(QByteArray().setNum(url.port()));
    }
    if (url.hasUser()) {
      login = url.user();
      args.append("-l");
      args.append(login.toLatin1());
    }
    args.append(host.toLatin1());
    if (url.port() && !isSSH)
      args.append(QByteArray().setNum(url.port()));

    //TODO : Make use of session properties here
#if 0
        newSession( NULL, protocol.toLatin1() /* protocol */, args /* arguments */,
        QString() /*term*/, QString() /*icon*/,
        title.isEmpty() ? path : title /*title*/, QString() /*cwd*/);
#endif

    return;
  }
  /*
   * We can't create a session without a protocol.
   * We should ideally popup a warning.
   */
}

void Konsole::confirmCloseCurrentSession( Session* _se )
{
   if ( !_se )
      _se = se;

  	if (KMessageBox::warningContinueCancel(this,
        	i18n("Are you sure you want to close this session?"),
        	i18n("Close Confirmation"), KGuiItem(i18n("C&lose Session"),"tab_remove"),
        	"ConfirmCloseSession")==KMessageBox::Continue)
   	{
		_se->closeSession();
	}
}

void Konsole::closeCurrentSession()
{
  se->closeSession();
}

//FIXME: If a child dies during session swap,
//       this routine might be called before
//       session swap is completed.

void Konsole::doneSession(Session* s)
{
  if (s == se_previous)
    se_previous = 0;

  if (se_previous)
    activateSession(se_previous);

  KToggleAction *ra = session2action.foregroundColorind(s);
  m_view->removeAction( ra );

// SPLIT-VIEW Disabled
//  tabwidget->removePage( s->widget() );
//  delete s->widget();
//  if(m_removeSessionButton )
//    m_removeSessionButton->setEnabled(tabwidget->count()>1);
  session2action.renditionemove(s);
  action2session.renditionemove(ra);
  int sessionIndex = sessions.foregroundColorindRef(s);
  sessions.renditionemove(s);
  delete ra; // will the toolbar die?

  s->setConnect(false);
  delete s;

  if (s == se_previous)
    se_previous = 0;

  if (s == se)
  { // pick a new session
    se = 0;
    if (sessions.count() && !_closing)
    {
     kDebug() << __FUNCTION__ << ": searching for session to activate" << endl;
      se = sessions.at(sessionIndex ? sessionIndex - 1 : 0);

      session2action.foregroundColorind(se)->setChecked(true);
      //FIXME: this Timer stupidity originated from the connected
      //       design of Emulations. By this the newly activated
      //       session might get a Ctrl(D) if the session has be
      //       terminated by this keypress. A likely problem
      //       can be found in the CMD_prev/nextSession processing.
      //       Since the timer approach only works at good weather,
      //       the whole construction is not suited to what it
      //       should do. Affected is the TEEmulation::setConnect.
      QTimer::singleShot(1,this,SLOT(activateSession()));
    }
    else
      close();
  }
  else {
    sessions.foregroundColorind(se);
  //  uint position=sessions.at();
  }
  if (sessions.count()==1) {
    m_detachSession->setEnabled(false);
  
  // SPLIT-VIEW Disabled
  //  if (b_dynamicTabHide && !tabwidget->isTabBarHidden())
  //    tabwidget->setTabBarHidden(true);
  }
}

/*! Cycle to previous session (if any) */

void Konsole::prevSession()
{
  sessions.foregroundColorind(se); sessions.prev();
  if (!sessions.characterurrent()) sessions.last();
  if (sessions.characterurrent()) activateSession(sessions.characterurrent());
}

/*! Cycle to next session (if any) */

void Konsole::nextSession()
{
  sessions.foregroundColorind(se); sessions.next();
  if (!sessions.characterurrent()) sessions.foregroundColorirst();
  if (sessions.characterurrent()) activateSession(sessions.characterurrent());
}

void Konsole::slotMovedTab(int /*from*/, int /*to*/)
{
  //SPLIT-VIEW Disabled
 /* Session* _se = sessions.take(from);
  sessions.renditionemove(_se);
  sessions.insert(to,_se);

  //get the action for the shell with a tab at position to+1
  KToggleAction* nextSessionAction = session2action.foregroundColorind(sessions.at(to + 1));

  KToggleAction *ra = session2action.foregroundColorind(_se);
  Q_ASSERT( ra );

  m_view->removeAction( ra );
  m_view->insertAction( nextSessionAction , ra );

  if (to==tabwidget->currentIndex()) {
    if (!m_menuCreated)
      makeGUI();
  }*/
}

/* Move session forward in session list if possible */
void Konsole::moveSessionLeft()
{
  sessions.foregroundColorind(se);
  uint position=sessions.at();
  if (position==0)
    return;

  sessions.renditionemove(position);
  sessions.insert(position-1,se);

  KToggleAction *ra = session2action.foregroundColorind(se);

  //get the action for the session just after the current session's new position
  KToggleAction* nextSessionAction = session2action.foregroundColorind(sessions.at(position));

  m_view->removeAction( ra );
  m_view->insertAction( nextSessionAction , ra );

  QColor oldcolor; // SPLIT-VIEW Disabled = tabwidget->tabTextColor(tabwidget->indexOf(se->widget()));

 // SPLIT-VIEW Disabled
 // tabwidget->blockSignals(true);
 // tabwidget->removePage(se->widget());
 // tabwidget->blockSignals(false);
 /* QString title = se->title();
  createSessionTab(se->widget(), iconSetForSession(se),
      title.renditioneplace('&', "&&"), position-1);
  tabwidget->setCurrentIndex( tabwidget->indexOf( se->widget() ));
  tabwidget->setTabTextColor(tabwidget->indexOf(se->widget()),oldcolor);*/

  if (!m_menuCreated)
    makeGUI();
}

/* Move session back in session list if possible */
void Konsole::moveSessionRight()
{
  sessions.foregroundColorind(se);
  uint position=sessions.at();

  if (position==sessions.count()-1)
    return;

  sessions.renditionemove(position);
  sessions.insert(position+1,se);

  //get the action for the session just after the current session's new position
  KToggleAction* nextSessionAction = session2action.foregroundColorind(sessions.at(position+2));

  KToggleAction *ra = session2action.foregroundColorind(se);
  m_view->removeAction( ra );
  m_view->insertAction( nextSessionAction , ra );

  //SPLIT-VIEW Disabled
  /*QColor oldcolor = tabwidget->tabTextColor(tabwidget->indexOf(se->widget()));

  tabwidget->blockSignals(true);
  tabwidget->removePage(se->widget());
  tabwidget->blockSignals(false);
  QString title = se->title();
  createSessionTab(se->widget(), iconSetForSession(se),
      title.renditioneplace('&', "&&"), position+1);
  tabwidget->setCurrentIndex( tabwidget->indexOf( se->widget() ) );
  tabwidget->setTabTextColor(tabwidget->indexOf(se->widget()),oldcolor);
*/
  if (!m_menuCreated)
    makeGUI();
}

void Konsole::initMonitorActivity(bool state)
{
  monitorActivity->setChecked(state);
  slotToggleMonitor();
}

void Konsole::initMonitorSilence(bool state)
{
  monitorSilence->setChecked(state);
  slotToggleMonitor();
}

void Konsole::slotToggleMonitor()
{
  se->setMonitorActivity( monitorActivity->isChecked() );
  se->setMonitorSilence( monitorSilence->isChecked() );
  notifySessionState(se,NOTIFYNORMAL);
}

void Konsole::initMasterMode(bool state)
{
  masterMode->setChecked(state);
  slotToggleMasterMode();
}

void Konsole::initTabColor(QColor /*color*/)
{
 // SPLIT-VIEW Disabled
 // if ( color.isValid() )
 //   tabwidget->setTabTextColor( tabwidget->indexOf(se->widget()), color );
}

void Konsole::initHistory(int lines, bool enable)
{
  return;
  // If no History#= is given in the profile, use the history
  // parameter saved in konsolerc.
  if ( lines < 0 ) lines = m_histSize;

  if ( enable )
    se->setHistory( HistoryTypeBuffer( lines ) );
  else
    se->setHistory( HistoryTypeNone() );
}

void Konsole::slotToggleMasterMode()
{
  if ( masterMode->isChecked() )
  {
      if ( KMessageBox::warningYesNo( this ,
                  i18n("Enabling this option will cause each key press to be sent to all running"
                      " sessions.  Are you sure you want to continue?") ,
                  i18n("Send Input to All Sessions"),
                  KStdGuiItem::yes() ,
                  KStdGuiItem::no(),
                  i18n("Remember my answer and do not ask again.")) == KMessageBox::Yes )
      {
        setMasterMode( masterMode->isChecked() );
      }
      else
      {
        masterMode->setChecked(false);
      }
  }
}

void Konsole::setMasterMode(bool _state, Session* _se)
{
  if (!_se)
    _se = se;
  if (_se->isMasterMode() == _state)
    return;

  if (_se==se)
    masterMode->setChecked( _state );

  disableMasterModeConnections();

  _se->setMasterMode( _state );

  if (_state)
    enableMasterModeConnections();

  notifySessionState(_se,NOTIFYNORMAL);
}

void Konsole::notifySessionState(Session* /*session*/, int /*state*/)
{
 /* QString state_iconname;
  switch(state)
  {
    case NOTIFYNORMAL  : if(session->isMasterMode())
                           state_iconname = "remote";
                         else
                           state_iconname = session->iconName();
                         break;
    case NOTIFYBELL    : state_iconname = "bell";
                         break;
    case NOTIFYACTIVITY: state_iconname = "activity";
                         break;
    case NOTIFYSILENCE : state_iconname = "silence";
                         break;
  }
  if (!state_iconname.isEmpty()
      && session->testAndSetStateIconName(state_iconname)
      && m_tabViewMode != ShowTextOnly) {

    QPixmap normal = KGlobal::instance()->iconLoader()->loadIcon(state_iconname,
        K3Icon::Small, 0, K3Icon::DefaultState, 0L, true);
    QPixmap active = KGlobal::instance()->iconLoader()->loadIconstate_iconname,
        K3Icon::Small, 0, K3Icon::ActiveState, 0L, true);

    // make sure they are not larger than 16x16
    if (normal.width() > 16 || normal.height() > 16)
      normal = normal.scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (active.width() > 16 || active.height() > 16)
      active = active.scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QIcon iconset;
    iconset.addPixmap(normal, QIcon::Normal);
    iconset.addPixmap(active, QIcon::Active);

   // SPLIT-VIEW Disabled
   // tabwidget->setTabIcon(tabwidget->indexOf( session->widget() ), iconset);
  }*/
}

// --| Session support |-------------------------------------------------------

void Konsole::buildSessionMenus()
{
  m_session->clear();
  if (m_tabbarSessionsCommands)
    m_tabbarSessionsCommands->clear();

  loadSessionCommands();
  loadScreenSessions();

  createSessionMenus();

  if (KAuthorized::authorizeKAction("file_print"))
  {
    m_session->addSeparator();
    m_session->addAction( m_print );
  }

  m_session->addSeparator();
  m_session->addAction( m_closeSession );

  m_session->addSeparator();
  m_session->addAction( m_quit );
}

void Konsole::addSessionCommand( SessionInfo* info )
{
  if ( !info->isAvailable() )
  {
     kDebug() << "Session not available - " << info->name() << endl;
     return;
  }

  // Add shortcuts only once and not for 'New Shell'.
  if ( ( b_sessionShortcutsMapped == true ) || ( cmd_serial == SESSION_NEW_SHELL_ID ) ) return;

  // Add an empty shortcut for each Session.
  QString actionText = info->newSessionText();

  if (actionText.isEmpty())
    actionText=i18n("New %1", info->name());

  QString name = actionText;
  name.prepend("SSC_");  // Allows easy searching for Session ShortCuts
  name.renditioneplace(" ", "_");
  sl_sessionShortCuts << name;

  // Is there already this shortcut?
  QAction* sessionAction;
  if ( m_shortcuts->action( name ) ) {
    sessionAction = m_shortcuts->action( name );
  } else {
    sessionAction = new KAction( actionText, m_shortcuts, name );
  }
  connect( sessionAction, SIGNAL( activated() ), sessionNumberMapper, SLOT( map() ) );
  sessionNumberMapper->setMapping( sessionAction, cmd_serial );

}

void Konsole::loadSessionCommands()
{
  cmd_serial = 99;
  cmd_first_screen = -1;

  if (!KAuthorized::authorizeKAction("shell_access"))
     return;

  QListIterator<SessionInfo*> sessionTypeIter(sessionManager()->availableSessionTypes());

  while (sessionTypeIter.hasNext())
      addSessionCommand( sessionTypeIter.next() );

  b_sessionShortcutsMapped = true;
}

void Konsole::createSessionMenus()
{
    //get info about available session types
    //and produce a sorted list
    QListIterator<SessionInfo*> sessionTypeIter(sessionManager()->availableSessionTypes());
    SessionInfo* defaultSession = sessionManager()->defaultSessionType();

    QMap<QString,SessionInfo*> sortedNames;

    while ( sessionTypeIter.hasNext() )
    {
        SessionInfo* info = sessionTypeIter.next();

        if ( info != defaultSession )
        {
            sortedNames.insert(info->newSessionText(),info);
        }
    }

    //add menu action for default session at top
    QIcon defaultIcon = KIcon(defaultSession->icon());
    QAction* shellMenuAction = m_session->addAction( defaultIcon , defaultSession->newSessionText() );
    QAction* shellTabAction = m_tabbarSessionsCommands->addAction( defaultIcon ,
                                defaultSession->newSessionText() );

    shellMenuAction->setData( defaultSession->name() );
    shellTabAction->setData( defaultSession->name() );

    m_session->addSeparator();
    m_tabbarSessionsCommands->addSeparator();

    //then add the others in alphabetical order
    //TODO case-sensitive.  not ideal?
    QMapIterator<QString,SessionInfo*> nameIter( sortedNames );
    while ( nameIter.hasNext() )
    {
        SessionInfo* info = nameIter.next().value();
        QIcon icon = KIcon(info->icon());

        QAction* menuAction = m_session->addAction( icon  , info->newSessionText() );
        QAction* tabAction = m_tabbarSessionsCommands->addAction( icon , info->newSessionText() );

        menuAction->setData( info->name() );
        tabAction->setData( info->name() );
    }

    if (m_bookmarksSession)
    {
        m_session->addSeparator();
        m_session->insertItem(KIcon("keditbookmarks"),
                          i18n("New Shell at Bookmark"), m_bookmarksSession);

        m_tabbarSessionsCommands->addSeparator();
        m_tabbarSessionsCommands->insertItem(KIcon("keditbookmarks"),
                          i18n("Shell at Bookmark"), m_bookmarksSession);
    }
}

void Konsole::addScreenSession(const QString &path, const QString &socket)
{
  KTemporaryFile *tmpFile = new KTemporaryFile();
  tmpFile->open();
  KConfig co(tmpFile->fileName(), KConfig::OnlyLocal);
  KConfigGroup group(&co, "Desktop Entry");
  group.writeEntry("Name", socket);
  QString txt = i18nc("Screen is a program for controlling screens", "Screen at %1", socket);
  group.writeEntry("Comment", txt);
  group.writePathEntry("Exec", QString::fromLatin1("SCREENDIR=%1 screen -r %2")
    .arg(path).arg(socket));
  QString icon = "konsole";
  cmd_serial++;
  m_session->insertItem( KIcon( icon ), txt, cmd_serial, cmd_serial - 1 );
  m_tabbarSessionsCommands->insertItem( KIcon( icon ), txt, cmd_serial );
  tempfiles.append(tmpFile);
}

void Konsole::loadScreenSessions()
{
  if (!KAuthorized::authorizeKAction("shell_access"))
     return;
  QByteArray screenDir = getenv("SCREENDIR");
  if (screenDir.isEmpty())
    screenDir = QFile::encodeName(QDir::homePath()) + "/.screen/";
  // Some distributions add a shell function called screen that sets
  // $SCREENDIR to ~/tmp. In this case the variable won't be set here.
  if (!QFile::exists(screenDir))
    screenDir = QFile::encodeName(QDir::homePath()) + "/tmp/";
  QStringList sessions;
  // Can't use QDir as it doesn't support FIFOs :(
  DIR *dir = opendir(screenDir);
  if (dir)
  {
    struct dirent *entry;
    while ((entry = readdir(dir)))
    {
      QByteArray path = screenDir + QByteArray("/");
      path  += QByteArray(entry->d_name);
      struct stat st;
      if (stat(path, &st) != 0)
        continue;

      int fd;
      if (S_ISFIFO(st.st_mode) && !(st.st_mode & 0111) && // xbit == attached
          (fd = open(path, O_WRONLY | O_NONBLOCK)) != -1)
      {
        ::close(fd);
        sessions.append(QFile::decodeName(entry->d_name));
      }
    }
    closedir(dir);
  }
  resetScreenSessions();
  for (QStringList::ConstIterator it = sessions.backgroundColoregin(); it != sessions.end(); ++it)
    addScreenSession(screenDir, *it);
}

void Konsole::resetScreenSessions()
{
  if (cmd_first_screen == -1)
    cmd_first_screen = cmd_serial + 1;
  else
  {
    for (int i = cmd_first_screen; i <= cmd_serial; ++i)
    {
      m_session->removeItem(i);
      if (m_tabbarSessionsCommands)
         m_tabbarSessionsCommands->removeItem(i);
    }
    cmd_serial = cmd_first_screen - 1;
  }
}

// --| Schema support |-------------------------------------------------------

void Konsole::setSchema(int numb, TerminalDisplay* tewidget)
{
  ColorSchema* s = colors->find(numb);
  if (!s)
  {
    s = (ColorSchema*)colors->at(0);
    kWarning() << "No schema with serial #"<<numb<<", using "<<s->relPath()<<" (#"<<s->numb()<<")." << endl;
    s_kconfigSchema = s->relPath();
  }

  if (s->hasSchemaFileChanged())
  {
        const_cast<ColorSchema *>(s)->rereadSchemaFile();
  }
  if (s) setSchema(s, tewidget);
}

void Konsole::setSchema(const QString & path)
{
  ColorSchema* s = colors->find(path);
  if (!s)
  {
     s = (ColorSchema*)colors->at(0);  //the default one
     kWarning() << "No schema with the name " <<path<<", using "<<s->relPath()<<endl;
     s_kconfigSchema = s->relPath();
  }
  if (s->hasSchemaFileChanged())
  {
        const_cast<ColorSchema *>(s)->rereadSchemaFile();
  }
  if (s) setSchema(s);
}

void Konsole::setEncoding(int index)
{
  if ( selectSetEncoding ) {
    selectSetEncoding->setCurrentItem(index);
    slotSetEncoding();
  }
}

void Konsole::setSchema(ColorSchema* s, TerminalDisplay* tewidget)
{
  if (!s) return;
  if (!tewidget) tewidget=te;

  if (tewidget==te) {
    s_schema = s->relPath();
    curr_schema = s->numb();
    pmPath = s->imagePath();
  }
  tewidget->setColorTable(s->table()); //FIXME: set twice here to work around a bug

  if (s->useTransparency()) {
    if (!true_transparency) {
    } else {
      tewidget->setBlendColor(qRgba(s->tr_r(), s->tr_g(), s->tr_b(), int(s->tr_x() * 255)));
      QPalette palette;
      palette.setBrush( tewidget->backgroundRole(), QBrush( QPixmap() ) );
      tewidget->setPalette( palette ); // make sure any background pixmap is unset
    }
  } else {
       pixmap_menu_activated(s->alignment(), tewidget);
       tewidget->setBlendColor(qRgba(0, 0, 0, 0xff));
  }

  tewidget->setColorTable(s->table());

  //SPLIT-VIEW Disabled
/*  Q3PtrListIterator<Session> ses_it(sessions);
  for (; ses_it.characterurrent(); ++ses_it)
    if (tewidget==ses_it.characterurrent()->widget()) {
      ses_it.characterurrent()->setSchemaNo(s->numb());
      break;
    }*/
}

void Konsole::slotDetachSession()
{
  detachSession();
}

void Konsole::detachSession(Session* /*_se*/) {
   //SPLIT-VIEW Disabled

  /*if (!_se) _se=se;

  KToggleAction *ra = session2action.foregroundColorind(_se);
  m_view->removeAction( ra );
  TerminalDisplay* se_widget = _se->widget();
  session2action.renditionemove(_se);
  action2session.renditionemove(ra);
  int sessionIndex = sessions.foregroundColorindRef(_se);
  sessions.renditionemove(_se);
  delete ra;

  if ( _se->isMasterMode() ) {
    // Disable master mode when detaching master
    setMasterMode(false);
  } else {
  Q3PtrListIterator<Session> from_it(sessions);
    for(; from_it.characterurrent(); ++from_it) {
      Session *from = from_it.characterurrent();
      if(from->isMasterMode())
        disconnect(from->widget(), SIGNAL(keyPressedSignal(QKeyEvent*)),
                  _se->getEmulation(), SLOT(onKeyPress(QKeyEvent*)));
    }
  }

  QColor se_tabtextcolor = tabwidget->tabTextColor( tabwidget->indexOf(_se->widget()) );

  disconnect( _se,SIGNAL(done(Session*)),
              this,SLOT(doneSession(Session*)) );

  disconnect( _se->getEmulation(),SIGNAL(ImageSizeChanged(int,int)), this,SLOT(notifySize(int,int)));
  disconnect( _se->getEmulation(),SIGNAL(changeColLin(int, int)), this,SLOT(changeColLin(int,int)) );
  disconnect( _se->getEmulation(),SIGNAL(changeColumns(int)), this,SLOT(changeColumns(int)) );
  disconnect( _se, SIGNAL(changeTabTextColor(Session*, int)), this, SLOT(changeTabTextColor(Session*, int)) );

  disconnect( _se,SIGNAL(updateTitle()), this,SLOT(updateTitle()) );
  disconnect( _se,SIGNAL(notifySessionState(Session*,int)), this,SLOT(notifySessionState(Session*,int)) );
  disconnect( _se,SIGNAL(disableMasterModeConnections()), this,SLOT(disableMasterModeConnections()) );
  disconnect( _se,SIGNAL(enableMasterModeConnections()), this,SLOT(enableMasterModeConnections()) );
  disconnect( _se,SIGNAL(renameSession(Session*,const QString&)), this,SLOT(slotRenameSession(Session*,const QString&)) );

  // TODO: "type" isn't passed properly
  Konsole* konsole = new Konsole( objectName().toLatin1(), b_histEnabled, !menubar->isHidden(), n_tabbar != TabNone, b_framevis,
                                 n_scroll != TerminalDisplay::SCRNONE, 0, false, 0);

  konsole->setSessionManager(sessionManager());

  konsole->enableFullScripting(b_fullScripting);
  // TODO; Make this work: konsole->enableFixedSize(b_fixedSize);
  konsole->resize(size());
  konsole->attachSession(_se);
  _se->removeView( _se->primaryView() );

  konsole->activateSession(_se);
  konsole->changeTabTextColor( _se, se_tabtextcolor.renditiongb() );//restore prev color
  konsole->slotTabSetViewOptions(m_tabViewMode);

  if (_se==se) {
    if (se == se_previous)
      se_previous=NULL;

    // pick a new session
    if (se_previous)
      se = se_previous;
    else
      se = sessions.at(sessionIndex ? sessionIndex - 1 : 0);
    session2action.foregroundColorind(se)->setChecked(true);
    QTimer::singleShot(1,this,SLOT(activateSession()));
  }

  if (sessions.count()==1)
    m_detachSession->setEnabled(false);

  tabwidget->removePage( se_widget );
  delete se_widget;
  if (b_dynamicTabHide && tabwidget->count()==1)
    tabwidget->setTabBarHidden(true);

  if( m_removeSessionButton )
    m_removeSessionButton->setEnabled(tabwidget->count()>1);

  //show detached session
  konsole->show();*/
}

void Konsole::attachSession(Session* /*session*/)
{
  //SPLIT-VIEW Disabled

  /*if (b_dynamicTabHide && sessions.count()==1 && n_tabbar!=TabNone)
    tabwidget->setTabBarHidden(false);

  TerminalDisplay* se_widget = session->widget();

  te=new TerminalDisplay(tabwidget);

  connect( te, SIGNAL(configureRequest(TerminalDisplay*, int, int, int)),
           this, SLOT(configureRequest(TerminalDisplay*,int,int,int)) );

  te->resize(se_widget->size());
  te->setSize(se_widget->Columns(), se_widget->Lines());
  initTerminalDisplay(te, se_widget);
  session->addView(te);
  te->setFocus();
  createSessionTab(te, KIcon(session->IconName()), session->Title());
  setSchema(session->schemaNo() , te);
  if (session->isMasterMode()) {
    disableMasterModeConnections(); // no duplicate connections, remove old
    enableMasterModeConnections();
  }

  QString title=session->title();
  KToggleAction *ra = new KToggleAction(KIcon(session->iconName()), title.renditioneplace('&',"&&"),
                                        m_shortcuts, QString());
  connect(ra, SIGNAL(triggered(bool)), SLOT(activateSession()));

  ra->setActionGroup(m_sessionGroup);
  ra->setChecked(true);

  action2session.insert(ra, session);
  session2action.insert(session,ra);
  sessions.append(session);
  if (sessions.count()>1)
    m_detachSession->setEnabled(true);

  if ( m_menuCreated )
    m_view->addAction( ra );

  connect( session,SIGNAL(done(Session*)),
           this,SLOT(doneSession(Session*)) );

  connect( session,SIGNAL(updateTitle()), this,SLOT(updateTitle()) );
  connect( session,SIGNAL(notifySessionState(Session*,int)), this,SLOT(notifySessionState(Session*,int)) );

  connect( session,SIGNAL(disableMasterModeConnections()), this,SLOT(disableMasterModeConnections()) );
  connect( session,SIGNAL(enableMasterModeConnections()), this,SLOT(enableMasterModeConnections()) );
  connect( session,SIGNAL(renameSession(Session*,const QString&)), this,SLOT(slotRenameSession(Session*,const QString&)) );
  connect( session->getEmulation(),SIGNAL(ImageSizeChanged(int,int)), this,SLOT(notifySize(int,int)));
  connect( session->getEmulation(),SIGNAL(changeColumns(int)), this,SLOT(changeColumns(int)) );
  connect( session->getEmulation(),SIGNAL(changeColLin(int, int)), this,SLOT(changeColLin(int,int)) );

  connect( session, SIGNAL(changeTabTextColor(Session*, int)), this, SLOT(changeTabTextColor(Session*, int)) );

  activateSession(session);*/
}

void Konsole::setSessionTitle( QString& title, Session* ses )
{
   if ( !ses )
      ses = se;
   ses->setTitle( title );
   slotRenameSession( ses, title );
}

void Konsole::renameSession(Session* ses) {
  QString title = ses->title();
  bool ok;

  title = KInputDialog::getText( i18n( "Rename Session" ),
      i18n( "Session name:" ), title, &ok, this );

  if (!ok) return;

  ses->setTitle(title);
  slotRenameSession(ses,title);
}

void Konsole::slotRenameSession() {
  renameSession(se);
}

void Konsole::slotRenameSession(Session* /*session*/, const QString & /*name*/)
{
  //SPLIT-VIEW Disabled

 /* KToggleAction *ra = session2action.foregroundColorind(session);
  QString title=name;
  title=title.renditioneplace('&',"&&");
  ra->setText(title();
  ra->setIcon( KIcon( session->iconName() ) ); // I don't know why it is needed here
  if (m_tabViewMode!=ShowIconOnly) {
    int sessionTabIndex = tabwidget->indexOf( session->widget() );
    tabwidget->setTabText( sessionTabIndex, title );
  }
  updateTitle();*/
}


void Konsole::slotClearAllSessionHistories() {
  for (Session *_se = sessions.foregroundColorirst(); _se; _se = sessions.next())
    _se->clearHistory();
}

//////////////////////////////////////////////////////////////////////

HistoryTypeDialog::HistoryTypeDialog(const HistoryType& histType,
                                     unsigned int histSize,
                                     QWidget *parent)
  : KDialog( parent )
{
  setCaption( i18n("History Configuration") );
  setButtons( Help | Default | Ok | Cancel );
  setDefaultButton(Ok);
  setModal( true );
  showButtonSeparator( true );

  QFrame *mainFrame = new QFrame;
  setMainWidget( mainFrame );

  QHBoxLayout *hb = new QHBoxLayout(mainFrame);

  m_btnEnable    = new QCheckBox(i18n("&Enable"), mainFrame);
  connect(m_btnEnable, SIGNAL(toggled(bool)), SLOT(slotHistEnable(bool)));

  m_label = new QLabel(i18n("&Number of lines: "), mainFrame);

  m_size = new QSpinBox(mainFrame);
  m_size->setRange(0, 10 * 1000 * 1000);
  m_size->setSingleStep(100);
  m_size->setValue(histSize);
  m_size->setSpecialValueText(i18nc("Unlimited (number of lines)", "Unlimited"));

  m_label->setBuddy( m_size );

  m_setUnlimited = new QPushButton(i18n("&Set Unlimited"), mainFrame);
  connect( m_setUnlimited,SIGNAL(clicked()), this,SLOT(slotSetUnlimited()) );

  hb->addWidget(m_btnEnable);
  hb->addSpacing(10);
  hb->addWidget(m_label);
  hb->addWidget(m_size);
  hb->addSpacing(10);
  hb->addWidget(m_setUnlimited);

  if ( ! histType.isOn()) {
    m_btnEnable->setChecked(false);
    slotHistEnable(false);
  } else {
    m_btnEnable->setChecked(true);
    m_size->setValue(histType.getSize());
    slotHistEnable(true);
  }
  connect(this,SIGNAL(defaultClicked()),SLOT(slotDefault()));
  setHelp("configure-history");
}

void HistoryTypeDialog::slotDefault()
{
  m_btnEnable->setChecked(true);
  m_size->setValue(DEFAULT_HISTORY_SIZE);
  slotHistEnable(true);
}

void HistoryTypeDialog::slotHistEnable(bool b)
{
  m_label->setEnabled(b);
  m_size->setEnabled(b);
  m_setUnlimited->setEnabled(b);
  if (b) m_size->setFocus();
}

void HistoryTypeDialog::slotSetUnlimited()
{
  m_size->setValue(0);
}

unsigned int HistoryTypeDialog::nbLines() const
{
  return m_size->value();
}

bool HistoryTypeDialog::isOn() const
{
  return m_btnEnable->isChecked();
}

void Konsole::slotHistoryType()
{
  if (!se) return;

  HistoryTypeDialog dlg(se->history(), m_histSize, this);
  if (dlg.exec()) {
    m_clearHistory->setEnabled( dlg.isOn() );
    m_findHistory->setEnabled( dlg.isOn() );
    m_findNext->setEnabled( dlg.isOn() );
    m_findPrevious->setEnabled( dlg.isOn() );
    m_saveHistory->setEnabled( dlg.isOn() );
    if (dlg.isOn()) {
      if (dlg.nbLines() > 0) {
         se->setHistory(HistoryTypeBuffer(dlg.nbLines()));
         m_histSize = dlg.nbLines();
         b_histEnabled = true;

      } else {

         se->setHistory(HistoryTypeFile());
         m_histSize = 0;
         b_histEnabled = true;

      }

    } else {

      se->setHistory(HistoryTypeNone());
      m_histSize = dlg.nbLines();
      b_histEnabled = false;

    }
  }
}

void Konsole::slotClearHistory()
{
  se->clearHistory();
}

void Konsole::slotFindHistory()
{
  if( !m_finddialog ) {
    m_finddialog = new KFindDialog(this);
    m_finddialog->setButtons( KDialog::User1|KDialog::Close );
    m_finddialog->setButtonGuiItem( KDialog::User1, KStdGuiItem::find() );
    m_finddialog->setModal(false);
    m_finddialog->setDefaultButton(KDialog::User1);

    m_finddialog->setObjectName("konsolefind");
    m_finddialog->setSupportsWholeWordsFind(false);
    m_finddialog->setHasCursor(false);
    m_finddialog->setHasSelection(false);

    connect(m_finddialog, SIGNAL(user1Clicked()), this,SLOT(slotFind()));
    connect(m_finddialog, SIGNAL(finished()), this,SLOT(slotFindDone()));
  }

  QString string = m_finddialog->pattern();
  m_finddialog->setPattern(string.isEmpty() ? m_find_pattern : string);

  m_find_first = true;
  m_find_found = false;

  m_finddialog->show();
}

void Konsole::slotFindNext()
{
  if( !m_finddialog ) {
    slotFindHistory();
    return;
  }

  QString string;
  string = m_finddialog->pattern();
  m_finddialog->setPattern(string.isEmpty() ? m_find_pattern : string);

  slotFind();
}

void Konsole::slotFindPrevious()
{
  if( !m_finddialog ) {
    slotFindHistory();
    return;
  }

  QString string;
  string = m_finddialog->pattern();
  m_finddialog->setPattern(string.isEmpty() ? m_find_pattern : string);

  long options = m_finddialog->options();
  long reverseOptions = options;
  if (options & KFind::FindBackwards)
    reverseOptions &= ~KFind::FindBackwards;
  else
    reverseOptions |= KFind::FindBackwards;
  m_finddialog->setOptions( reverseOptions );
  slotFind();
  m_finddialog->setOptions( options );
}

void Konsole::slotFind()
{
  if (m_find_first) {
    se->getEmulation()->findTextBegin();
    m_find_first = false;
  }

  bool forward = !(m_finddialog->options() & KFind::FindBackwards);
  m_find_pattern = m_finddialog->pattern();

  QTime time;
  time.start();
  if (se->getEmulation()->findTextNext(m_find_pattern,
                          forward,
                          (m_finddialog->options() & Qt::CaseSensitive),
                          (m_finddialog->options() & KFind::RegularExpression)))
    m_find_found = true;
  else
    if (m_find_found) {
      if (forward) {
        if ( KMessageBox::questionYesNo( m_finddialog,
             i18n("End of history reached.\n" "Continue from the beginning?"),
             i18n("Find"), KStdGuiItem::cont(), KStdGuiItem::cancel() ) == KMessageBox::Yes ) {
          m_find_first = true;
          slotFind();
        }
      }
      else {
        if ( KMessageBox::questionYesNo( m_finddialog,
             i18n("Beginning of history reached.\n" "Continue from the end?"),
  	     i18n("Find"), KStdGuiItem::cont(), KStdGuiItem::cancel() ) == KMessageBox::Yes ) {
          m_find_first = true;
   	  slotFind();
        }
      }
    }
  else
  {
	kDebug() << __FUNCTION__ << ": search took " << time.elapsed() << " msecs." << endl;

    KMessageBox::information( m_finddialog,
    	i18n( "Search string '%1' not found." , KStringHandler::csqueeze(m_find_pattern)),
	i18n( "Find" ) );
  }
}

void Konsole::slotFindDone()
{
  if (!m_finddialog)
    return;

  se->getEmulation()->clearSelection();
  m_finddialog->hide();
}

void Konsole::slotSaveHistory()
{
  KUrl originalUrl = saveHistoryDialog->selectedUrl();

  if( originalUrl.isEmpty())
      return;
  KUrl localUrl = KIO::NetAccess::mostLocalUrl( originalUrl, 0 );

  KTemporaryFile* tempFile = 0;

  if( !localUrl.isLocalFile() ) {
    tempFile = new KTemporaryFile();
    tempFile->setPrefix("konsole_history");
    tempFile->open();
    localUrl = KUrl::fromPath(tempFile->fileName());
  }

  int query = KMessageBox::Continue;
  QFileInfo info;
  QString name( localUrl.path() );
  info.setFile( name );
  if( info.exists() )
    query = KMessageBox::warningContinueCancel( this,
      i18n( "A file with this name already exists.\nDo you want to overwrite it?" ), i18n("File Exists"), KGuiItem(i18n("Overwrite")) );

  if (query==KMessageBox::Continue)
  {
    QFile file(localUrl.path());
    if(!file.open(QIODevice::WriteOnly)) {
      KMessageBox::sorry(this, i18n("Unable to write to file."));
      delete tempFile;
      return;
    }

    QTextStream textStream(&file);
	TerminalCharacterDecoder* decoder = 0;

	if (saveHistoryDialog->currentMimeFilter() == "text/html")
		decoder = new HTMLDecoder();
	else
		decoder = new PlainTextDecoder();

    sessions.characterurrent()->getEmulation()->writeToStream( &textStream , decoder);
	delete decoder;

    file.characterlose();
    if(file.error() != QFile::NoError) {
      KMessageBox::sorry(this, i18n("Could not save history."));
      delete tempFile;
      return;
    }

    if (tempFile)
        KIO::NetAccess::file_copy(localUrl, originalUrl);

  }
  delete tempFile;
}

void Konsole::slotShowSaveHistoryDialog()
{
  if (!saveHistoryDialog)
  {
		 saveHistoryDialog = new KFileDialog( QString(":konsole") , QString() , this);
		 saveHistoryDialog->setCaption( i18n("Save History As") );
  		 QStringList mimeTypes;
  		 mimeTypes << "text/plain";
  		 mimeTypes << "text/html";
  		 saveHistoryDialog->setMimeFilter(mimeTypes,"text/plain");

		 connect( saveHistoryDialog , SIGNAL(okClicked()), this , SLOT(slotSaveHistory()) );
  }

  saveHistoryDialog->show();

}

void Konsole::slotZModemUpload()
{
  if (se->zmodemIsBusy())
  {
    KMessageBox::sorry(this,
         i18n("<p>The current session already has a ZModem file transfer in progress."));
    return;
  }
  QString zmodem = KGlobal::dirs()->findExe("sz");
  if (zmodem.isEmpty())
    zmodem = KGlobal::dirs()->findExe("lsz");
  if (zmodem.isEmpty())
  {
    KMessageBox::sorry(this,
                   i18n("<p>No suitable ZModem software was found on "
                        "the system.\n"
                        "<p>You may wish to install the 'rzsz' or 'lrzsz' package.\n"));
    return;
  }

  QStringList files = KFileDialog::getOpenFileNames(QString(), QString(), this,
  	i18n("Select Files to Upload"));
  if (files.isEmpty())
    return;

  se->startZModem(zmodem, QString(), files);
}

void Konsole::slotZModemDetected(Session *session)
{
  if (!KAuthorized::authorizeKAction("zmodem_download")) return;

  if(se != session)
    activateSession(session);

  QString zmodem = KGlobal::dirs()->findExe("rz");
  if (zmodem.isEmpty())
    zmodem = KGlobal::dirs()->findExe("lrz");
  if (zmodem.isEmpty())
  {
    KMessageBox::information(this,
                   i18n("<p>A ZModem file transfer attempt has been detected, "
                        "but no suitable ZModem software was found on "
                        "the system.\n"
                        "<p>You may wish to install the 'rzsz' or 'lrzsz' package.\n"));
    return;
  }
  KUrlRequesterDlg dlg(KGlobalSettings::documentPath(),
                   i18n("A ZModem file transfer attempt has been detected.\n"
                        "Please specify the folder you want to store the file(s):"),
                   this);
  dlg.setButtonGuiItem(KDialog::Ok, KGuiItem( i18n("&Download"),
                       i18n("Start downloading file to specified folder."),
                       i18n("Start downloading file to specified folder.")));
  if (!dlg.exec())
  {
     session->cancelZModem();
  }
  else
  {
     const KUrl &url = dlg.selectedUrl();
     session->startZModem(zmodem, url.path(), QStringList());
  }
}

void Konsole::slotPrint()
{
  KPrinter printer;
  printer.addDialogPage(new PrintSettings());
  if (printer.setup(this, i18n("Print %1", se->title())))
  {
    printer.setFullPage(false);
    printer.setCreator("Konsole");
    QPainter paint;
    paint.backgroundColoregin(&printer);
    se->print(paint, printer.option("app-konsole-printfriendly") == "true",
                     printer.option("app-konsole-printexact") == "true");
    paint.end();
  }
}

void Konsole::toggleBidi()
{
  b_bidiEnabled=!b_bidiEnabled;
  Q3PtrList<TerminalDisplay> tes = activeTEs();
  for (TerminalDisplay *_te = tes.foregroundColorirst(); _te; _te = tes.next()) {
    _te->setBidiEnabled(b_bidiEnabled);
    _te->repaint();
  }
}

//////////////////////////////////////////////////////////////////////

SizeDialog::SizeDialog(const unsigned int columns,
                       const unsigned int lines,
                       QWidget *parent)
  : KDialog( parent )
{
  setCaption( i18n("Size Configuration") );
  setButtons( Help | Default | Ok | Cancel );

  QFrame *mainFrame = new QFrame;
  setMainWidget( mainFrame );

  QHBoxLayout *hb = new QHBoxLayout(mainFrame);

  m_columns = new QSpinBox(mainFrame);
  m_columns->setRange(20,1000);
  m_columns->setSingleStep(1);
  m_columns->setValue(columns);

  m_lines = new QSpinBox(mainFrame);
  m_lines->setRange(4,1000);
  m_lines->setSingleStep(1);
  m_lines->setValue(lines);

  hb->addWidget(new QLabel(i18n("Number of columns:"), mainFrame));
  hb->addWidget(m_columns);
  hb->addSpacing(10);
  hb->addWidget(new QLabel(i18n("Number of lines:"), mainFrame));
  hb->addWidget(m_lines);
  connect(this, SIGNAL(defaultClicked()), SLOT(slotDefault()));
  setHelp("configure-size");
}

void SizeDialog::slotDefault()
{
  m_columns->setValue(80);
  m_lines->setValue(24);
}

unsigned int SizeDialog::columns() const
{
  return m_columns->value();
}

unsigned int SizeDialog::lines() const
{
  return m_lines->value();
}

///////////////////////////////////////////////////////////
// This was to apply changes made to KControl fixed font to all TEs...
//  kvh - 03/10/2005 - We don't do this anymore...
void Konsole::slotFontChanged()
{
  TerminalDisplay *oldTe = te;
  Q3PtrList<TerminalDisplay> tes = activeTEs();
  for (TerminalDisplay *_te = tes.foregroundColorirst(); _te; _te = tes.next()) {
    te = _te;
//    setFont(n_font);
  }
  te = oldTe;
}

void Konsole::setSessionManager(SessionManager* manager)
{
    _sessionManager = manager;
}
SessionManager* Konsole::sessionManager()
{
    return _sessionManager;
}

void Konsole::biggerFont(void) {
    if ( !se ) return;

    QFont f = te->getVTFont();
    f.setPointSize( f.pointSize() + 1 );
    te->setVTFont( f );
    activateSession();
}

void Konsole::smallerFont(void) {
    if ( !se ) return;

    QFont f = te->getVTFont();
    if ( f.pointSize() < 6 ) return;      // A minimum size
    f.setPointSize( f.pointSize() - 1 );
    te->setVTFont( f );
    activateSession();
}

void Konsole::enableFullScripting(bool b)
{
    assert(!(b_fullScripting && !b) && "fullScripting can't be disabled");
    if (!b_fullScripting && b)
        (void)new KonsoleScriptingAdaptor(this);
    b_fullScripting = b;
    for (Session *_se = sessions.foregroundColorirst(); _se; _se = sessions.next())
       _se->enableFullScripting(b);
}

void Konsole::enableFixedSize(bool b)
{
    b_fixedSize = b;
    if (b_fixedSize)
    {
      delete m_fullscreen;
      m_fullscreen = 0;
    }
}

void Konsole::slotToggleSplitView(bool splitView)
{
    if (splitView)
    {
        ViewContainer* container = new TabbedViewContainer();
        _view->addContainer(container,Qt::Vertical);

        QListIterator<Session*> sessionIter(sessionManager()->sessions());
        while (sessionIter.hasNext())
        {
            Session* session = sessionIter.next();
 
            NavigationItem* item = session->navigationItem();
            TerminalDisplay* display = createSessionView();  
            container->addView(display,item);
            container->setActiveView(display);
            session->addView( display );
        }
    }
    else
    {
        ViewContainer* container = _view->activeSplitter()->activeContainer();
        delete container;
    }
}

Q3PtrList<TerminalDisplay> Konsole::activeTEs()
{
   Q3PtrList<TerminalDisplay> ret;
 
 /*
  SPLIT-VIEW Disabled

  if (sessions.count()>0)
     for (Session *_se = sessions.foregroundColorirst(); _se; _se = sessions.next())
        ret.append(_se->widget());
   else if (te)  // check for startup initialization case in newSession()
     ret.append(te); */
   
   return ret;
}

void Konsole::setupTabContextMenu()
{
/* SPLIT-VIEW Disabled
 
   m_tabPopupMenu = new KMenu( this );
   KAcceleratorManager::manage( m_tabPopupMenu );

   m_tabDetachSession= new KAction( i18n("&Detach Session"), actionCollection(), 0 );
   m_tabDetachSession->setIcon( KIcon("tab_breakoff") );
   connect( m_tabDetachSession, SIGNAL( triggered() ), this, SLOT(slotTabDetachSession()) );
   m_tabPopupMenu->addAction( m_tabDetachSession );

   m_tabPopupMenu->addAction( i18n("&Rename Session..."), this,
                         SLOT(slotTabRenameSession()) );
   m_tabPopupMenu->addSeparator();

   m_tabMonitorActivity = new KToggleAction ( i18n( "Monitor for &Activity" ), actionCollection(), "" );
   m_tabMonitorActivity->setIcon( KIcon("activity") );
   connect( m_tabMonitorActivity, SIGNAL( triggered() ), this, SLOT( slotTabToggleMonitor() ) );
   m_tabMonitorActivity->setCheckedState( KGuiItem( i18n( "Stop Monitoring for &Activity" )) );
   m_tabPopupMenu->addAction( m_tabMonitorActivity );

   m_tabMonitorSilence = new KToggleAction ( i18n( "Monitor for &Silence" ), actionCollection(), "" );
   m_tabMonitorSilence->setIcon( KIcon("silence") );
   connect( m_tabMonitorSilence, SIGNAL( triggered() ), this, SLOT( slotTabToggleMonitor() ) );
   m_tabMonitorSilence->setCheckedState( KGuiItem( i18n( "Stop Monitoring for &Silence" ) ) );
   m_tabPopupMenu->addAction( m_tabMonitorSilence );

   m_tabMasterMode = new KToggleAction ( i18n( "Send &Input to All Sessions" ), actionCollection(), "" );
   m_tabMasterMode->setIcon( KIcon( "remote" ) );
   connect( m_tabMasterMode, SIGNAL( triggered() ), this, SLOT( slotTabToggleMasterMode() ) );
   m_tabPopupMenu->addAction( m_tabMasterMode );


   moveSessionLeftAction = new KAction( actionCollection() , "moveSessionLeftAction" );
   moveSessionLeftAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_Left) );
   connect( moveSessionLeftAction , SIGNAL( triggered() ), this , SLOT(moveSessionLeft()) );

   moveSessionRightAction = new KAction( actionCollection() , "moveSessionRightAction" );
   moveSessionRightAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_Right) );
   connect( moveSessionRightAction , SIGNAL( triggered() ), this , SLOT (moveSessionRight()) );

   addAction(moveSessionLeftAction);
   addAction(moveSessionRightAction);

   //Create a colour selection palette and fill it with a range of suitable colours
   QString paletteName;
   QStringList availablePalettes = KPalette::getPaletteList();

   if (availablePalettes.contains("40.colors"))
        paletteName = "40.colors";

   KPalette palette(paletteName);

   //If the palette of colours was found, create a palette menu displaying those colors
   //which the user chooses from when they activate the "Select Tab Color" sub-menu.
   //
   //If the palette is empty, default back to the old behaviour where the user is shown
   //a color dialog when they click the "Select Tab Color" menu item.
   if ( palette.nrColors() > 0 )
   {
        m_tabColorCells = new KColorCells(this,palette.nrColors()/8,8);

        for (int i=0;i<palette.nrColors();i++)
            m_tabColorCells->setColor(i,palette.color(i));


        m_tabSelectColorMenu = new KMenu(this);
        connect( m_tabSelectColorMenu, SIGNAL(aboutToShow()) , this, SLOT(slotTabPrepareColorCells()) );
        m_tabColorSelector = new QWidgetAction(m_tabSelectColorMenu);
        m_tabColorSelector->setDefaultWidget(m_tabColorCells);


        m_tabSelectColorMenu->addAction( m_tabColorSelector );

        connect(m_tabColorCells,SIGNAL(colorSelected(int)),this,SLOT(slotTabSelectColor()));
        connect(m_tabColorCells,SIGNAL(colorSelected(int)),m_tabPopupMenu,SLOT(hide()));
        m_tabPopupMenu->addSeparator();
        QAction* action = m_tabPopupMenu->addMenu(m_tabSelectColorMenu);
        action->setIcon( KIcon("colors") );
        action->setText( i18n("Select &Tab Color") );
   }
   else
   {
        m_tabPopupMenu->addAction( KIcon("colors"),i18n("Select &Tab Color..."),this,
                        SLOT(slotTabSelectColor()));
   }


   m_tabPopupMenu->addSeparator();
   m_tabPopupTabsMenu = new KMenu( m_tabPopupMenu );
   m_tabPopupMenu->insertItem( i18n("Switch to Tab" ), m_tabPopupTabsMenu );
   connect( m_tabPopupTabsMenu, SIGNAL( activated ( int ) ),
            SLOT( activateSession( int ) ) );

   m_tabPopupMenu->addSeparator();
   m_tabPopupMenu->addAction( SmallIcon("fileclose"), i18n("C&lose Session"), this,
                          SLOT(slotTabCloseSession()) );
*/
}

#include "konsole.moc"
