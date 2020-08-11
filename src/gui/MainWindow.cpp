#include <QtWidgets/QFileDialog>
#include "MainWindow.h"
#include "DisplayGrid.h"
#include <Document.h>
#include <QtWidgets/QLabel>
#include <include/QSSPreprocessor.h>
#include <include/Properties.h>
#include <include/Globals.h>
#include <iostream>
#include <QtGui/QtGui>
#include <include/QHBoxWidget.h>
#include <QApplication>
#include <QMessageBox>
#include <QComboBox>
#include <include/RaytraceView.h>
#include <include/AboutWindow.h>
#include <include/HelpWidget.h>


using namespace BRLCAD;
using namespace std;



MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    loadTheme();
    prepareUi();
    prepareDockables();


    if(QCoreApplication::arguments().length()>1){
        openFile(QString(QCoreApplication::arguments().at(1)));
    }
    Globals::mainWindow = this;
}

MainWindow::~MainWindow()
{
    for (const std::pair<const int, Document *> pair: documents){
        Document * document = pair.second;
        delete document;
    }
}

void MainWindow::loadTheme()
{
	QFile themeFile( ":themes/arbalest_light.theme" );
	themeFile.open( QFile::ReadOnly );
	QString themeStr( themeFile.readAll() );
	Globals::theme = new QSSPreprocessor(themeStr);
	themeFile.close();

	QFile styleFile( ":styles/arbalest_simple.qss" );
	styleFile.open( QFile::ReadOnly );
	QString styleStr(styleFile.readAll() );
	qApp->setStyleSheet(Globals::theme->process(styleStr));
	styleFile.close();
}

void MainWindow::prepareUi() {
    setWindowFlags(Qt::FramelessWindowHint);    // Hide window title bar
    setWindowTitle("Arbalest");
	// Menu bar -------------------------------------------------------------------------------------------------------------
    menuTitleBar = new QMenuBar(this);
    menuTitleBar->installEventFilter(this);
    setMenuBar(menuTitleBar);

    QMenu *fileMenu = menuTitleBar->addMenu(tr("&File"));

    QAction* newAct = new QAction(tr("&New"), this);
    newAct->setShortcuts(QKeySequence::New);
    newAct->setStatusTip(tr("New .g file"));
    connect(newAct, &QAction::triggered, this, &MainWindow::newFile);
    fileMenu->addAction(newAct);

    QAction* openAct = new QAction(tr("&Open"), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Opens a .g file"));
    connect(openAct, &QAction::triggered, this, &MainWindow::openFileDialog);
    fileMenu->addAction(openAct);

    QAction* saveAct = new QAction(tr("Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save database"));
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveFileDefaultPath);
    fileMenu->addAction(saveAct);

    QAction* saveAsAct = new QAction(tr("Save As..."), this);
    saveAsAct->setShortcuts(QKeySequence::SaveAs);
    saveAsAct->setStatusTip(tr("Save database as"));
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveAsFileDialog);
    fileMenu->addAction(saveAsAct);


    QMenu* viewMenu = menuTitleBar->addMenu(tr("&View"));

    QAction* resetViewportAct = new QAction("Reset current viewport", this);
    resetViewportAct->setStatusTip(tr("Reset to default camera orientation for the viewport and autoview to currently visible objects"));
    connect(resetViewportAct, &QAction::triggered, this, [this](){
        if (activeDocumentId == -1) return;
        documents[activeDocumentId]->getDisplayGrid()->resetViewPort(documents[activeDocumentId]->getDisplayGrid()->getActiveDisplayId());
    });
    viewMenu->addAction(resetViewportAct);

    QAction* resetAllViewportsAct = new QAction("Reset all viewports", this);
    resetAllViewportsAct->setStatusTip(tr("Reset to default camera orientation for each viewport and autoview to visible objects"));
    connect(resetAllViewportsAct, &QAction::triggered, this, [this](){
        if (activeDocumentId == -1) return;
        documents[activeDocumentId]->getDisplayGrid()->resetAllViewPorts();
    });
    viewMenu->addAction(resetAllViewportsAct);

    viewMenu->addSeparator();

    QAction* autoViewAct = new QAction(tr("Focus visible objects (all viewports)"), this);
    autoViewAct->setShortcut(Qt::Key_F|Qt::CTRL);
    autoViewAct->setStatusTip(tr("Resize and center the view based on the current visible objects"));
    connect(autoViewAct, &QAction::triggered, this, [this](){
        if (activeDocumentId == -1) return;
        for(Display * display : documents[activeDocumentId]->getDisplayGrid()->getDisplays()){
            display->getCamera()->autoview();
            display->forceRerenderFrame();
        }
    });
    viewMenu->addAction(autoViewAct);

    QAction* autoViewSingleAct = new QAction(tr("Focus visible objects (current viewport)"), this);
    autoViewSingleAct->setStatusTip(tr("Resize and center the view based on the current visible objects"));
    connect(autoViewSingleAct, &QAction::triggered, this, [this](){
        if (activeDocumentId == -1) return;
        documents[activeDocumentId]->getDisplay()->getCamera()->autoview();
        documents[activeDocumentId]->getDisplay()->forceRerenderFrame();
    });
    viewMenu->addAction(autoViewSingleAct);

    QAction* centerViewAct = new QAction(tr("Focus selected object"), this);
    centerViewAct->setStatusTip(tr("Resize and center the view based on the selected objects"));
    centerViewAct->setShortcut(Qt::Key_F);
    connect(centerViewAct, &QAction::triggered, this, [this](){
        if (activeDocumentId == -1) return;
        int objectId = documents[activeDocumentId]->getObjectTreeWidget()->currentItem()->data(0, Qt::UserRole).toInt();
        documents[activeDocumentId]->getDisplay()->getCamera()->centerView(objectId);
    });
    viewMenu->addAction(centerViewAct);
    
    viewMenu->addSeparator();

    currentViewport = new QComboBox();

    QMenu* singleView = viewMenu->addMenu(tr("&Single View"));

    QActionGroup *viewportMenuGroup = new QActionGroup(this);
    for(int i=0;i<4;i++) {
        singleViewAct[i] = new QAction("Viewport " + QString::number(i+1), this);
        viewportMenuGroup->addAction(singleViewAct[i]);
        singleViewAct[i]->setCheckable(true);
        singleViewAct[i]->setStatusTip("Display viewport " + QString::number(i+1));
        connect(singleViewAct[i], &QAction::triggered, this, [this,i]() {
            if (activeDocumentId == -1) return;
            documents[activeDocumentId]->getDisplayGrid()->singleDisplayMode(i);
            currentViewport->setCurrentIndex(i);
        });
        singleView->addAction(singleViewAct[i]);
    }
    singleViewAct[0]->setShortcut(Qt::Key_1);
    singleViewAct[1]->setShortcut(Qt::Key_2);
    singleViewAct[2]->setShortcut(Qt::Key_3);
    singleViewAct[3]->setShortcut(Qt::Key_4);
    singleViewAct[3]->setChecked(true);

    QAction* quadViewAct = new QAction(tr("All Viewports"), this);
    quadViewAct->setStatusTip(tr("Display 4 viewports"));
    connect(quadViewAct, &QAction::triggered, this, [this](){
        if (activeDocumentId == -1) return;
        documents[activeDocumentId]->getDisplayGrid()->quadDisplayMode();
        for(QAction *i : singleViewAct) i->setChecked(false);
        currentViewport->setCurrentIndex(4);
    });
    quadViewAct->setShortcut(Qt::Key_5);
    viewMenu->addAction(quadViewAct);

    viewMenu->addSeparator();
    
    QAction* toggleGridAct = new QAction(tr("Toggle grid on/off"), this);
    toggleGridAct->setShortcut(Qt::Key_G);
    connect(toggleGridAct, &QAction::triggered, this, [this](){
        if (activeDocumentId == -1) return;
        documents[activeDocumentId]->getDisplayGrid()->getActiveDisplay()->gridEnabled = 
                !documents[activeDocumentId]->getDisplayGrid()->getActiveDisplay()->gridEnabled;
        documents[activeDocumentId]->getDisplayGrid()->getActiveDisplay()->forceRerenderFrame();
    });
    viewMenu->addAction(toggleGridAct);


    QMenu* raytrace = menuTitleBar->addMenu(tr("&Raytrace"));
    QAction* raytraceAct = new QAction(tr("Raytrace current viewport"), this);
    raytraceAct->setStatusTip(tr("Raytrace current viewport"));
    raytraceAct->setShortcut(Qt::CTRL|Qt::Key_R);
    connect(raytraceAct, &QAction::triggered, this, [this](){
        if (activeDocumentId == -1) return;
        statusBar->showMessage("Raytracing current viewport...", statusBarShortMessageDuration);
        QCoreApplication::processEvents();
        documents[activeDocumentId]->getRaytraceWidget()->raytrace();
        statusBar->showMessage("Raytracing completed.", statusBarShortMessageDuration);
    });
    raytrace->addAction(raytraceAct);


    QMenu* help = menuTitleBar->addMenu(tr("&Help"));
    QAction* aboutAct = new QAction(tr("About"), this);
    connect(aboutAct, &QAction::triggered, this, [this](){
        (new AboutWindow())->show();
    });
    help->addAction(aboutAct);

    QAction* helpAct = new QAction(tr("Help"), this);
    helpAct->setShortcut(Qt::Key_F1);
    connect(helpAct, &QAction::triggered, this, [this](){
        HelpWidget * helpWidget = dynamic_cast<HelpWidget*>(documentArea->widget(0));
        if (helpWidget== nullptr){
            documentArea->insertTab(0,new HelpWidget,"Quick Start");
        }
        documentArea->setCurrentIndex(0);
    });
    help->addAction(helpAct);

    // Title bar [widgets in the menu bar] ----------------------------------------------------------------------------------------
    QPushButton* applicationIcon = new QPushButton(menuTitleBar);
    applicationIcon->setIcon(QIcon(":/icons/arbalest_icon.png"));
    applicationIcon->setObjectName("topLeftAppIcon");
    menuTitleBar->setCornerWidget(applicationIcon, Qt::TopLeftCorner);
    setWindowIcon(*new QIcon(*new QBitmap(":/icons/arbalest_icon.png")));

    QHBoxLayout* layoutTopRightWidget = new QHBoxLayout;
    layoutTopRightWidget->setContentsMargins(0, 0, 0, 0);
    QWidget* topRightWidget = new QWidget;
    topRightWidget->setLayout(layoutTopRightWidget);
    menuTitleBar->setCornerWidget(topRightWidget, Qt::TopRightCorner);
    layoutTopRightWidget->setSpacing(0);

    QPushButton* minimizeButton = new QPushButton(topRightWidget);
    minimizeButton->setIcon(QIcon(":/icons/minimize.png"));
    minimizeButton->setObjectName("minimizeButton");
    connect(minimizeButton, &QPushButton::clicked, this, &MainWindow::minimizeButtonPressed);
    layoutTopRightWidget->addWidget(minimizeButton);


    maximizeButton = new QPushButton(topRightWidget);
    if (this->windowState() == Qt::WindowMaximized) {
        maximizeButton->setIcon(QIcon(":/icons/restore_down.png"));
    }
    else {
        maximizeButton->setIcon(QIcon(":/icons/maximize.png"));
    }
    maximizeButton->setObjectName("maximizeButton");
    connect(maximizeButton, &QPushButton::clicked, this, &MainWindow::maximizeButtonPressed);
    layoutTopRightWidget->addWidget(maximizeButton);

    QPushButton* closeButton = new QPushButton(topRightWidget);
    closeButton->setIcon(QIcon(":/icons/close.png"));
    closeButton->setObjectName("closeButton");
    connect(closeButton, &QPushButton::clicked, this, &MainWindow::closeButtonPressed);
    layoutTopRightWidget->addWidget(closeButton);

	// Status bar ----------------------------------------------------------------------------------------------------------
    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);
    statusBarPathLabel = new QLabel("No document open");
    statusBarPathLabel->setObjectName("statusBarPathLabel");
    statusBar->addWidget(statusBarPathLabel);
	

    // Document area --------------------------------------------------------------------------------------------------------
    documentArea = new QTabWidget(this);
    documentArea->setObjectName("documentArea");
    documentArea->setMovable(true);
    documentArea->setTabsClosable(true);
    setCentralWidget(documentArea);
    documentArea->tabBar()->setObjectName("documentAreaTabBar");
    connect(documentArea, &QTabWidget::currentChanged, this, &MainWindow::onActiveDocumentChanged);
    connect(documentArea, &QTabWidget::tabCloseRequested, this, &MainWindow::tabCloseRequested);
	
    QHBoxWidget * mainTabBarCornerWidget = new QHBoxWidget();
    mainTabBarCornerWidget->setObjectName("mainTabBarCornerWidget");

    QPushButton* newButton = new QPushButton(menuTitleBar);
    newButton->setIcon(QPixmap::fromImage(QImage(":/icons/icons8-new-file-80.png")));
    newButton->setObjectName("toolbarButton");
    newButton->setToolTip("New..");
    mainTabBarCornerWidget->addWidget(newButton);
    connect(newButton, &QPushButton::clicked, this, &MainWindow::newFile);

    QPushButton* openButton = new QPushButton(menuTitleBar);
    openButton->setIcon(QPixmap::fromImage(QImage(":/icons/icons8-opened-folder-80.png")));
    openButton->setObjectName("toolbarButton");
    openButton->setToolTip("Open..");
    mainTabBarCornerWidget->addWidget(openButton);	
    connect(openButton, &QPushButton::clicked, this, &MainWindow::openFileDialog);

    QPushButton* saveButton = new QPushButton(menuTitleBar);
    saveButton->setIcon(QPixmap::fromImage(QImage(":/icons/icons8-save-80.png")));
    saveButton->setObjectName("toolbarButton");
    saveButton->setToolTip("Save");
    mainTabBarCornerWidget->addWidget(saveButton);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveFileDefaultPath);

    QPushButton* saveAsButton = new QPushButton(menuTitleBar);
    saveAsButton->setIcon(QPixmap::fromImage(QImage(":/icons/icons8-save-as-80.png")));
    saveAsButton->setObjectName("toolbarButton");
    saveAsButton->setToolTip("Save as..");
    mainTabBarCornerWidget->addWidget(saveAsButton);
    connect(saveAsButton, &QPushButton::clicked, this, &MainWindow::saveAsFileDialog);

    mainTabBarCornerWidget->addWidget(toolbarSeparator(false));


    QPushButton* focusAll = new QPushButton(menuTitleBar);
    focusAll->setIcon(QPixmap::fromImage(QImage(":/icons/baseline_crop_free_black_48dp.png")));
    focusAll->setObjectName("toolbarButton");
    focusAll->setToolTip("Focus on all visible objects (Ctrl+F)");
    connect(focusAll, &QPushButton::clicked, this, [this](){
        if (activeDocumentId == -1) return;
        for(Display * display : documents[activeDocumentId]->getDisplayGrid()->getDisplays()){
            display->getCamera()->autoview();
            display->forceRerenderFrame();
        }
    });
    mainTabBarCornerWidget->addWidget(focusAll);

    QPushButton* focusCurrent = new QPushButton(menuTitleBar);
    focusCurrent->setIcon(QPixmap::fromImage(QImage(":/icons/baseline_center_focus_strong_black_48dp.png")));
    focusCurrent->setObjectName("toolbarButton");
    focusCurrent->setToolTip("Focus on selected object (F)");
    connect(focusCurrent, &QPushButton::clicked, this, [this](){
        if (activeDocumentId == -1) return;
        int objectId = documents[activeDocumentId]->getObjectTreeWidget()->currentItem()->data(0, Qt::UserRole).toInt();
        documents[activeDocumentId]->getDisplay()->getCamera()->centerView(objectId);
    });
    mainTabBarCornerWidget->addWidget(focusCurrent);


    QPushButton* resetViewports = new QPushButton(menuTitleBar);
    resetViewports->setIcon(QPixmap::fromImage(QImage(":/icons/baseline_refresh_black_48dp.png")));
    resetViewports->setObjectName("toolbarButton");
    resetViewports->setToolTip("Reset the viewports and focus on the visible");
    connect(resetViewports, &QPushButton::clicked, this, [this](){
        if (activeDocumentId == -1) return;
        documents[activeDocumentId]->getDisplayGrid()->resetAllViewPorts();
    });
    mainTabBarCornerWidget->addWidget(resetViewports);
    
    currentViewport->setToolTip("Change viewport");
    currentViewport->addItem("Viewport 1");
    currentViewport->addItem("Viewport 2");
    currentViewport->addItem("Viewport 3");
    currentViewport->addItem("Viewport 4");
    currentViewport->addItem("All Viewports");
    currentViewport->setCurrentIndex(3);
    connect(currentViewport, QOverload<int>::of(&QComboBox::activated),[=](int index){
        if (activeDocumentId == -1) return;
        if (index <4) documents[activeDocumentId]->getDisplayGrid()->singleDisplayMode(index);
        else documents[activeDocumentId]->getDisplayGrid()->quadDisplayMode();
        for(QAction *i : singleViewAct) i->setChecked(false);
        if(index != 4) singleViewAct[index]->setChecked(true);
    });
    mainTabBarCornerWidget->addWidget(currentViewport);



    QPushButton* toggleGrid = new QPushButton(menuTitleBar);
    toggleGrid->setIcon(QPixmap::fromImage(QImage(":/icons/sharp_grid_on_black_48dp.png")));
    toggleGrid->setObjectName("toolbarButton");
    toggleGrid->setToolTip("Toggle grid on/off (G)");
    connect(toggleGrid, &QPushButton::clicked, this, [this](){
        if (activeDocumentId == -1) return;
        documents[activeDocumentId]->getDisplayGrid()->getActiveDisplay()->gridEnabled =
                !documents[activeDocumentId]->getDisplayGrid()->getActiveDisplay()->gridEnabled;
        documents[activeDocumentId]->getDisplayGrid()->getActiveDisplay()->forceRerenderFrame();
    });
    mainTabBarCornerWidget->addWidget(toggleGrid);

    mainTabBarCornerWidget->addWidget(toolbarSeparator(false));

    QPushButton* raytraceButton = new QPushButton(menuTitleBar);
    raytraceButton->setIcon(QPixmap::fromImage(QImage(":/icons/icons8-teapot-40.png")));
    raytraceButton->setObjectName("toolbarButton");
    raytraceButton->setToolTip("Raytrace current viewport (Ctrl+R)");
    mainTabBarCornerWidget->addWidget(raytraceButton);
    connect(raytraceButton, &QPushButton::clicked, this, [this](){
        if (activeDocumentId == -1) return;
        statusBar->showMessage("Raytracing current viewport...", statusBarShortMessageDuration);
        QCoreApplication::processEvents();
        documents[activeDocumentId]->getRaytraceWidget()->raytrace();
        statusBar->showMessage("Raytracing completed.", statusBarShortMessageDuration);
    });

    documentArea->setCornerWidget(mainTabBarCornerWidget,Qt::Corner::TopRightCorner);

    documentArea->addTab(new HelpWidget(), "Quick Start");
}

void MainWindow::prepareDockables(){
    // Object tree
    objectTreeWidgetDockable = new Dockable("Objects", this, false, 200);
    addDockWidget(Qt::LeftDockWidgetArea, objectTreeWidgetDockable);

    // Properties
    objectPropertiesDockable = new Dockable("Properties", this,true,300);
    addDockWidget(Qt::RightDockWidgetArea, objectPropertiesDockable);



    // Toolbox
//    toolboxDockable = new Dockable("Make", this,true,30);
//    toolboxDockable->hideHeader();
//    addDockWidget(Qt::LeftDockWidgetArea, toolboxDockable);
}

// empty new file
void MainWindow::newFile() {
    Document* document = new Document(documentsCount);
    document->getObjectTreeWidget()->setObjectName("dockableContent");
    document->getProperties()->setObjectName("dockableContent");
    documents[documentsCount++] = document;
    QString filename( "Untitled");
    const int tabIndex = documentArea->addTab(document->getDisplayGrid(), filename);
    documentArea->setCurrentIndex(tabIndex);
    connect(documents[activeDocumentId]->getObjectTreeWidget(), &ObjectTreeWidget::selectionChanged,
            this, &MainWindow::objectTreeWidgetSelectionChanged);
    
}

void MainWindow::openFile(const QString& filePath) {
    Document* document = nullptr;

    try {
        document = new Document(documentsCount, &filePath);
    }
    catch (...) {
        QString msg = "Failed to open " + filePath;
        statusBar->showMessage(msg, statusBarShortMessageDuration);

        QMessageBox msgBox;
        msgBox.setText(msg);
        msgBox.exec();
    }

    if (document != nullptr) {
        document->getObjectTreeWidget()->setObjectName("dockableContent");
        document->getProperties()->setObjectName("dockableContent");
        documents[documentsCount++] = document;
        QString filename(QFileInfo(filePath).fileName());
        const int tabIndex = documentArea->addTab(document->getDisplayGrid(), filename);
        documentArea->setCurrentIndex(tabIndex);
        connect(documents[activeDocumentId]->getObjectTreeWidget(), &ObjectTreeWidget::selectionChanged,
                this, &MainWindow::objectTreeWidgetSelectionChanged);
    }
}

bool MainWindow::saveFile(const QString& filePath) {
    if (activeDocumentId == -1) return false;
    return documents[activeDocumentId]->getDatabase()->Save(filePath.toUtf8().data());
}

void MainWindow::openFileDialog()
{
	const QString filePath = QFileDialog::getOpenFileName(documentArea, tr("Open BRL-CAD database"), QString(), "BRL-CAD Database (*.g)");
    if (!filePath.isEmpty()){
        openFile(filePath);
    }
}

void MainWindow::saveAsFileDialog() {
    if (activeDocumentId == -1) return;
	const QString filePath = QFileDialog::getSaveFileName(this, tr("Save BRL-CAD database"), QString(), "BRL-CAD Database (*.g)");
    if (!filePath.isEmpty()) {
        if (saveFile(filePath))
        {
            documents[activeDocumentId]->setFilePath(filePath);
            QString filename(QFileInfo(filePath).fileName());
            documentArea->setTabText(documentArea->currentIndex(), filename);
            statusBarPathLabel->setText(*documents[activeDocumentId]->getFilePath());
            statusBar->showMessage("Saved to " + filePath, statusBarShortMessageDuration);
        }
    }
}

void MainWindow::saveFileDefaultPath() {
    if (activeDocumentId == -1) return;
    if (documents[activeDocumentId]->getFilePath() == nullptr) saveAsFileDialog();
    else {
        const QString filePath = *documents[activeDocumentId]->getFilePath();
        if (!filePath.isEmpty()) {
            if (saveFile(filePath))
            {
                statusBar->showMessage("Saved to " + filePath, statusBarShortMessageDuration);
            }
        }
    }
}

void MainWindow::onActiveDocumentChanged(const int newIndex){
    DisplayGrid * displayGrid = dynamic_cast<DisplayGrid*>(documentArea->widget(newIndex));
    if (displayGrid != nullptr && displayGrid->getDocument()->getDocumentId() != activeDocumentId){
        activeDocumentId = displayGrid->getDocument()->getDocumentId();
        objectTreeWidgetDockable->setContent(documents[activeDocumentId]->getObjectTreeWidget());
        objectPropertiesDockable->setContent(documents[activeDocumentId]->getProperties());
        statusBarPathLabel->setText(documents[activeDocumentId]->getFilePath()  != nullptr ? *documents[activeDocumentId]->getFilePath() : "Untitled");

        if(documents[activeDocumentId]->getDisplayGrid()->inQuadDisplayMode()){
            currentViewport->setCurrentIndex(4);
            for(QAction * action:singleViewAct) action->setChecked(false);
        }else {
            currentViewport->setCurrentIndex(documents[activeDocumentId]->getDisplayGrid()->getActiveDisplayId());
            for(QAction * action:singleViewAct) action->setChecked(false);
            singleViewAct[documents[activeDocumentId]->getDisplayGrid()->getActiveDisplayId()]->setChecked(true);
        }

    }
}

void MainWindow::tabCloseRequested(const int i)
{
    documentArea->removeTab(i);
    if (documentArea->currentIndex() == -1){
        objectTreeWidgetDockable->clear();
        objectPropertiesDockable->clear();
        activeDocumentId = -1;
        documentArea->addTab(new HelpWidget(), "Quick Start");
    }
}

void MainWindow::objectTreeWidgetSelectionChanged(int objectId) {
    documents[activeDocumentId]->getProperties()->bindObject(objectId);
}

void MainWindow::closeButtonPressed(){
    close();
}

void MainWindow::minimizeButtonPressed() {
    showMinimized();
}

void MainWindow::maximizeButtonPressed() {
    if(!isMaximized()) showMaximized();
    else showNormal();
}

// drag window by holding from menu bar (now functioning as title bar too)
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    static QPoint dragPosition{};
    if (watched == menuTitleBar)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent* mouse_event = dynamic_cast<QMouseEvent*>(event);
            if (mouse_event->button() == Qt::LeftButton)
            {
                dragPosition = mouse_event->globalPos() - frameGeometry().topLeft();
                return false;
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent* mouse_event = dynamic_cast<QMouseEvent*>(event);
            if (mouse_event->buttons() & Qt::LeftButton)
            {
                if(isMaximized()) return false;//showNormal();
                //todo showNormal when dragged
                move(mouse_event->globalPos() - dragPosition);
                return false;
            }
        }
    }
    return false;
}

void MainWindow::changeEvent( QEvent* e ) {
    if (e->type() == QEvent::WindowStateChange) {
        if (this->windowState() == Qt::WindowMaximized) {
            maximizeButton->setIcon(QIcon(":/icons/restore_down.png"));
        }
        else{
            maximizeButton->setIcon(QIcon(":/icons/maximize.png"));
        }
    }
}