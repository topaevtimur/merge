#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtWidgets>
#include <QtNetwork>
#include "QStandardItem"
#include "QStandardItemModel"
#include<iostream>
#include <map>
#include <String>
#include <vector>
#include <tlhelp32.h>

const int FIRST_TEAM = 0, SECOND_TEAM = 1, CAPTAIN = 0, PLAYER = 1;
const int BLACK = 0, RED = 1, BLUE = 2, GRAY = 3;
int myRole = 1,myTeam = 1;

int opened[10][10];

bool server = false;

int active_team = FIRST_TEAM, active_role = CAPTAIN;
QStandardItemModel *model;
QStandardItem *item;
QString tableWords[10][10];
int tableColor[10][10];
int active = 0;
typedef struct {
    QString name;
    QString uid;
    int team, role;
    long long last_activity;
}player;

 player m_p(QString uid, QString name){
        player g;
        g.name = name;
        g.uid = uid;
        g.role = CAPTAIN;
        return(g);
        }

std::map<QString, player > users;
std::map<QString, player> gameRoles;
player self;

QString userHash(){
    QProcess proc;
    proc.start("wmic bios get serialnumber",QIODevice::ReadWrite);
    proc.waitForFinished();
    qint64 some = QCoreApplication::applicationPid ();
    QString uID = QString::number(some);
    return QString("%1").arg(QString(QCryptographicHash::hash(uID.toUtf8(),QCryptographicHash::Md5).toHex()));
}

int getMyRole(){
    QString myHash = userHash();
    qDebug()<<gameRoles.size();
    if ( gameRoles.find(myHash) != gameRoles.end() ) {
        qDebug()<<"FIND IN";
        return gameRoles[myHash].role;
    }
    return(1);
}


void display(int role) {
    for(int i = 0; i < 5; i++)
                for(int j = 0; j < 5; j++) {
                    item = new QStandardItem(tableWords[i][j]);
                    //qDebug() << tableColor[i][j];
                    if(opened[i][j] == 1 || role == 0)
                    switch(tableColor[i][j]) {
                        case 0:
                            item ->setBackground(Qt::black);
                            item ->setForeground(Qt::white);
                            break;
                        case 1:
                            item ->setBackground(Qt::red);
                            break;
                        case 2:
                            item ->setBackground(Qt::blue);
                            break;
                        case 3:
                            item ->setBackground(Qt::gray);
                            break;

                    }
                             model->setItem(i, j, item);

              }
}

void MainWindow::on_tableView_clicked(const QModelIndex &index)
{
    int col = index.column(), row = index.row();

    //condition true for captain else for player;
    qDebug()<<getMyRole();
    //qDebug()<<gameRoles;

    QMessageBox msg;
    if(!getMyRole()) {
        msg.setText("Вы капитан");
        msg.exec();
        //do nothing;
    } else if (!active){
        msg.setText("Ход другого игрока");
        msg.exec();
    } else {
        // should be sent about click
        opened[row][col] = 1;
        active = 0;
        display(1);

//        item = new QStandardItem(QString("*"));
//        model->setItem(row,col,item);
//        ui->tableView->setModel(model);
        //std::cout<<col<<' '<<row<<std::endl<<"---"<<std::endl;

    }
    QString value = ui->tableView->model()->data(ui->tableView->model()->index(row,col)).toString();
    //std::cout<<col<<' '<<row<<std::endl<<" --- "<<value.toLocal8Bit().toStdString()<<std::endl;

}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->startGame->hide();
    ui->button_sent->hide();
    ui->text_task->hide();

    self = m_p(userHash(),ui->nickname->text());

    model = new QStandardItemModel;
    QFile file("words.txt");
    QByteArray data;
    if(!file.open(QIODevice::ReadOnly)) {
        //TODO smth bad with words.txt
       // return;
    }
    QString words[700];//685
    int cur = 0;
    while (!file.atEnd()) {
           words[cur++] = file.readLine();
    }
    QTime midnight(0,0,0);
    qsrand(midnight.secsTo(QTime::currentTime()));
    int col[4] = {1, 8, 9, 7};// black, red, blue, grey;
    for(int i = 0; i < 5; i++) {
        for(int j =0; j < 5; j++) {
            int tmp = qrand() % 4;
            while(col[tmp] <= 0)tmp = qrand() % 4;
            tableColor[i][j] = tmp;
            --col[tmp];
          //  qDebug() << tmp;
            tableWords[i][j] = words[qrand() % 685];
            //std::cout<<words[qrand() % 685].toLocal8Bit().toStdString()<<std::endl;
            item = new QStandardItem(tableWords[i][j]);
            model->setItem(i,j,item);



        }
    }
    //display(1);

    getMyRole();

    ui->tableView->setModel(model);
    ui->tableView->resizeRowsToContents();
    ui->tableView->resizeColumnsToContents();
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

}


MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::on_button_host_released() //Start server
{


    MainWindow::ui->button_host->hide();
    MainWindow::ui->button_join->hide();
    MainWindow::ui->startGame->show();

    //биндим отправку пакетов
    timerSC = new QTimer(this);

    udpSocketSC = new QUdpSocket(this);

    messageNo = 1;
    startBroadcastingSC();
    connect(timerSC, SIGNAL(timeout()), this, SLOT(broadcastDatagramSC()));

    //биндим прослушивание порта

    server = true;
    udpSocketSC = new QUdpSocket(this);
    udpSocketSC->bind(12121, QUdpSocket::ShareAddress);
    connect(udpSocketSC, SIGNAL(readyRead()),
            this, SLOT(processPendingDatagramsSC()));

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch(1);
    buttonLayout->addStretch(1);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);

    setWindowTitle(tr("Broadcast Receiver"));

}

void MainWindow::processPendingDatagramsSC() //from client
{
    while (udpSocketSC->hasPendingDatagrams()) {

        QByteArray datagram;
        datagram.resize(udpSocketSC->pendingDatagramSize());
        udpSocketSC->readDatagram(datagram.data(), datagram.size());
        std::cout << "Parsing "+tr("Received datagram: \"%1\"")
                     .arg(datagram.data()).toStdString()<<std::endl;
        ui->statusLabel_2->setText(tr("FROM CLIENT Received datagram: \"%1\"")
                             .arg(datagram.data()));
        QString fromClient = datagram.data();
        std::string str = fromClient.toStdString();
        std::string delimiter = "@";

        QString gg = QString::fromStdString(str);
        size_t pos = 0;
        std::string token;
        std::vector<QString> tokens;
        QStringList list = gg.split("@");
        if(list.size()==3)
        gg = list[0]+"-"+list[1]+"-"+list[2];
        QString timestamp = list[2];
        QString nickname = list[1];
        QString id = list[0];
        player to_push = m_p(id, nickname);
        to_push.last_activity = timestamp.toLongLong();
        users[id] = to_push;
        ui->statusMessages->setText("Message:\n");
        for(auto i : users){

            QString a = i.first;
            player b = i.second;
            QString res = ui->statusMessages->toPlainText()+"nickname: "+b.name+"\n";
            ui->statusMessages->setText(res);
                //std::cout<<a.toStdString() << " has value " << b.name.toStdString()<< std::endl;
        }
    }
//! [2]
}

void MainWindow::processPendingDatagramsCS() //response from server
{
//! [2]
    while (udpSocketCS->hasPendingDatagrams()) {

        QByteArray datagram;
        datagram.resize(udpSocketCS->pendingDatagramSize());
        udpSocketCS->readDatagram(datagram.data(), datagram.size());
        QString data = QTextCodec :: codecForLocale()->toUnicode(datagram);
       // QString data = datagram.data();
        qDebug() << data;
        QString type;
        QString field;
        QString status;
        QString player_list;
        QString team;
        QString role;
        QStringList list = data.split("#");
        if(list.size()<1){
            //std::cout<<"Something gone wrong :[ "<<list.size()<<std::endl;
            continue;
        }
        if(list[0]=="general"&&list.size()!=6){
            //std::cout<<"Something gone wrong with general :[ "<<list.size()<<std::endl;
        }
        type = list[0];
        field = list[1];
        QStringList fieldList = field.split("$");
        for(int i=0;i<fieldList.size();i++){
            QStringList pair = fieldList[i].split(".");
            QString color = pair[0];
            QString word = pair[1];
           // qDebug()<<i%5<<' '<<i/5;
            tableWords[i/5][i%5] = word;
            tableColor[i/5][i%5] = color.toLong();
            //item = new QStandardItem(tableWords[i%5][i/5]);
           // model->clear();
            //model->setItem(i/5,i%5,item);
            //qDebug()<<color<<' '<<word;
        }
        status = list[2];
        player_list = list[3];
        QStringList playerList = player_list.split("|");//Сделать парсер в gameRoles из списка пользователей!
        for(int i=0;i<playerList.size();i++){
            QString playerString = playerList[i];
            QStringList fields = playerString.split("@");
            qDebug()<<"-------"<<fields.size()<<' '<<playerString;
            if(fields.size()<4) continue;
            player pl = m_p(fields[0],fields[1]);
            pl.role = fields[2].toLong();
            pl.team = fields[3].toLong();
            gameRoles[fields[0]] = pl;
        }

        team = list[4];
        active_team = team.toLong();
        role = list[5];
        active_role = role.toLong();


        //std::cout<<list.size()<<std::endl;
        display(getMyRole());
        ui->statusLabel_2->setText(tr("FROM SERVER Received datagram: \"%1\"")
                             .arg(datagram.data()));
        myRole = self.role;
        myTeam = self.team;
        if(self.role == 0) {
            ui->button_sent->show();
            ui->text_task->show();
        }
        if(self.role == active_role && active_team == self.team) {
            QMessageBox msg;
            msg.setText("Ваш ход");
            msg.exec();
            active = 1;
        }
    }
//! [2]
}
void MainWindow::on_button_join_released()
{
    MainWindow::ui->button_host->hide();
    MainWindow::ui->button_join->hide();

    timerCS = new QTimer(this);
//! [0]
    udpSocketCS = new QUdpSocket(this);
//! [0]
    messageNo = 1;
    startBroadcastingCS();
    connect(timerCS, SIGNAL(timeout()), this, SLOT(broadcastDatagramCS()));
    //биндим считывание пакетов
    udpSocketCS = new QUdpSocket(this);
    udpSocketCS->bind(45454, QUdpSocket::ShareAddress);
    connect(udpSocketCS, SIGNAL(readyRead()),
            this, SLOT(processPendingDatagramsCS()));


}

void MainWindow::startBroadcastingSC()
{
    timerSC->start(2000);
}

void MainWindow::startBroadcastingCS()
{
    timerCS->start(2000);
}

void MainWindow::broadcastDatagramSC() // data to client
{
    ui->statusLabel->setText(tr("TO CLIENT Now broadcasting datagram %1").arg(messageNo));
    QString type = "general";
    QString field = "blue.word1$red.word2";
    for(int i=0;i<5;i++){
        for(int j=0;j<5;j++){
            tableWords[i][j] = tableWords[i][j].simplified();
            if(i+j==0)//i=0,j=0
                field = QString::number(tableColor[i][j])+"."+tableWords[i][j]+"."+QString::number(opened[i][j]);
            else field += "$"+QString::number(tableColor[i][j])+"."+tableWords[i][j]+"."+QString::number(opened[i][j]);
        }
    }
    QString game_status = "before_start";
    int myRole = 0;//после добавления генерации ролей изменить!!!
    QString player_list = "";
    QString team = QString::number(active_team);
    QString role = QString::number(active_role);
    int cnt = 0;
    for(auto i : gameRoles){
        QString a = i.first;
        player b = i.second;
        if(cnt != 0 )
            player_list += "|"+a+"@"+b.name+"@"+QString::number(b.role)+"@"+QString::number(b.team);
        else
            player_list += a+"@"+b.name+"@"+QString::number(b.role)+"@"+QString::number(b.team);
        cnt++;
    }
    QString result = type+"#"+field+"#"+game_status+"#"+player_list+"#"+team+"#"+role;
    qDebug()<<result;
    //std::cout<<result.toLocal8Bit().toStdString()<<std::endl;

    QByteArray datagram = result.toLocal8Bit();
    udpSocketSC->writeDatagram(datagram.data(), datagram.size(),
                             QHostAddress::Broadcast, 45454);
//! [1]
    ++messageNo;
}

void MainWindow::broadcastDatagramCS() //data to server
{

    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    QString hash = userHash();
    QString toResponse = hash+"@"+ui->nickname->text()+"@"+QString::number(timestamp);
    ui->statusLabel->setText("TO SERVER Now broadcasting: " +toResponse);
    QByteArray datagram = toResponse.toLocal8Bit();
    udpSocketCS->writeDatagram(datagram.data(), datagram.size(),
                             QHostAddress::Broadcast, 12121);
    if(false){
        //QByteArray choseCell = go_hard.toLocal8Bit();
        //udpSocketCS->writeDatagram(choseCell.data(), choseCell.size(),
         //                        QHostAddress::Broadcast, 12121);
    }
    ++messageNo;
}


void MainWindow::on_button_host_clicked()
{

}

void MainWindow::on_pushButton_released() //start game
{

}

int myrandom (int i) { return std::rand()%i;}

void MainWindow::on_startGame_released()
{
    //generation roles for current active players
   // qDebug()<<"---Start";
    std::vector<player> all;
    all.push_back(self);
    for(auto i : users){
        player toPush;
        toPush = m_p(i.first,i.second.name);
        //qDebug()<<toPush.uid<<' '<<toPush.name;
        all.push_back(toPush);
    }
    std::random_shuffle ( all.begin(), all.end(), myrandom);
    for(int i=0;i<all.size();i++){
        //std::cout<<all[i].uid.std::endl;
    }
    for(int i=0;i<all.size();i++){
        QString uid = all[i].uid;
        gameRoles[uid].name = all[i].name;
        switch(i){
            case 0:
            {
                gameRoles[uid].role = 0;
                gameRoles[uid].team = 0;
                break;
            }
            case 1:
            {
                gameRoles[uid].role = 0;
                gameRoles[uid].team = 1;
                break;
            }
            default:
                gameRoles[uid].role = 1;
                gameRoles[uid].team = i%2;
        }
    }
    for(auto i : gameRoles){
        QString a = i.first;
        player b = i.second;
        qDebug()<<a<<' '<<b.name<<' '<<b.role<<' '<<b.team;
    }
    //qDebug()<<"---Finish";

}

void MainWindow::on_button_sent_released()
{

    QMessageBox msg;
    if(!active) {
        msg.setText("Ход другого игрока");
        msg.exec();
    } else {
        // Отправить message
        active = 0;
    }

}
