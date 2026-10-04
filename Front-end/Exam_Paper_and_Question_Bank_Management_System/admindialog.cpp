#include "admindialog.h"
#include "ui_admindialog.h"
#include <QMessageBox>

AdminDialog::AdminDialog(QWidget *parent,std::unique_ptr<TCPClientSocket> client) :
    QDialog(parent),
    ui(new Ui::AdminDialog)
{
    ui->setupUi(this);
    this->client=std::move(client);
    // set the delete button to be non-touchable to prevent misuse
    ui->deleteButton->setEnabled(false);
}

AdminDialog::~AdminDialog()
{
    delete ui;
}

void AdminDialog::read_users(){
    ui->userListWidget->clear();
    // read users from the back-end and store the names in list
    json sendPacket=R"({"command":"get users"})"_json;
    if(client->sendToServer(sendPacket)==-1){
        QMessageBox::warning(this,"warning","send get users command failed");
    }
    json recvPacket;
    if(client->receive(recvPacket)==-1){
        QMessageBox::warning(this,"warning","receive user list failed");
    }
    int iter=0;
    QList<QString> users;
    if(recvPacket["code"]==200){
        iter=recvPacket["counts"];
        for(int i=0;i<iter;i++){
            if(client->receive(recvPacket)==-1){
                QMessageBox::warning(this,"warning","receive user names failed");
                return;
            }
            QString userName=QString::fromUtf8(std::string(recvPacket["username"]).c_str());
            users.append(userName);
        }
    }else{
        QMessageBox::warning(this,"warning","undefined message from server");
        return;
    }
    ui->userListWidget->addItems(users);
    ui->deleteButton->setEnabled(false);
}

void AdminDialog::delete_user(QString userName){
    if(userName.length()==0){
        QMessageBox::warning(this,"warning","please choose a user to delete");
        ui->deleteButton->setEnabled(false);
        return;
    }
    json sendPacket=json::parse(fmt::format("{{\"command\":\"delete user\",\"username\":\"{}\"}}",userName.toStdString()));
    if(client->sendToServer(sendPacket)==-1){
        QMessageBox::warning(this,"warning","fail to send to the server");
        return;
    }
    json recvPacket;
    if(client->receive(recvPacket)==-1){
        QMessageBox::warning(this,"warning","fail to receive feedback from the server");
        return;
    }
    if(recvPacket["code"]!=200){
        QMessageBox::warning(this,"warning","fail to delete user from the server");
        return;
    }
    read_users();
    ui->deletePrompt->setText("delete successfully");
}

void AdminDialog::register_user(QString userName, QString password){
    // check if the entered username or password is empty
    if(userName.trimmed().length()==0){
        ui->registerPrompt->setText("username is empty");
        ui->userName->setFocus();
        return;
    }
    if(password.trimmed().length()==0){
        ui->registerPrompt->setText("password is empty");
        ui->password->setFocus();
        return;
    }
    if(ui->checkBoxAdmin->isChecked()==false&&ui->checkBoxRuleMaker->isChecked()==false&&ui->checkBoxTeacher->isChecked()==false){
        ui->registerPrompt->setText("no identity is selected");
        return;
    }
    std::string rawJson;
    // encryption: QCryptographicHash::hash((password).toLocal8Bit(),QCryptographicHash::Sha3_512).toHex().toStdString()
    if(ui->checkBoxAdmin->isChecked())
        rawJson=fmt::format("{{\"command\": \"{}\", \"username\": \"{}\", \"password\": \"{}\",\"identity\":\"{}\"}}",
                                        "register user",userName.toStdString(),password.toStdString(),"admin");
    else if(ui->checkBoxRuleMaker->isChecked())
        rawJson=fmt::format("{{\"command\": \"{}\", \"username\": \"{}\", \"password\": \"{}\",\"identity\":\"{}\"}}",
                                        "register user",userName.toStdString(),password.toStdString(),"rule maker");
    else
        rawJson=fmt::format("{{\"command\": \"{}\", \"username\": \"{}\", \"password\": \"{}\",\"identity\":\"{}\"}}",
                                        "register user",userName.toStdString(),password.toStdString(),"teacher");
    json sendPacket=json::parse(rawJson);
    if(client->sendToServer(sendPacket)==-1){
        QMessageBox::warning(this, "warning", "fail to register user to server");
        return;
    }
    json recvPacket;
    if(client->receive(recvPacket)==-1){
        QMessageBox::warning(this, "warning", "fail to receive message from server");
        return;
    }
    if(recvPacket["code"]==403){
        QMessageBox::warning(this, "warning", "repeated user name");
        ui->registerPrompt->setText("register failed");
        return;

    }else if(recvPacket["code"]!=200){
        QMessageBox::warning(this, "warning", "undefined message from server");
        return;
    }
    read_users();
    ui->registerPrompt->setText("register successfully");
    ui->userName->clear();
    ui->password->clear();
}

void AdminDialog::on_registerButton_clicked(){
    std::chrono::duration<double,std::milli> dur;
    auto start=std::chrono::steady_clock::now();
    register_user(ui->userName->text(),ui->password->text());
    auto end=std::chrono::steady_clock::now();
    dur=end - start;
    std::cout<<"register user duration: "<<dur.count()<<" ms"<<std::endl;
}

void AdminDialog::on_exitButton_clicked()
{
    json sendPacket=json::parse(fmt::format("{{\"command\":\"logout\"}}"));
    if(client->sendToServer(sendPacket)==-1){
        QMessageBox::warning(this,"warning","fail to send to the server");
        return;
    }
    json recvPacket;
    if(client->receive(recvPacket)==-1){
        QMessageBox::warning(this,"warning","fail to receive feedback from the server");
        return;
    }
    if(recvPacket["code"]!=200){
        QMessageBox::warning(this,"warning","fail to log out from the server");
        return;
    }
    emit admin_panel_be_closed();
}

void AdminDialog::on_deleteButton_clicked()
{
    delete_user(ui->userListWidget->currentItem()->text().trimmed());
}


void AdminDialog::on_userListWidget_itemSelectionChanged()
{
    ui->deleteButton->setEnabled(true);
    ui->deletePrompt->clear();
}

void AdminDialog::open_admin_panel(){
    this->setWindowTitle("Window for system administrators");
    this->read_users();
    this->show();
}

void AdminDialog::close_admin_panel(){
    this->deleteLater();
    parentWidget()->show();
}

void AdminDialog::on_checkBoxAdmin_pressed()
{
    ui->checkBoxRuleMaker->setChecked(false);
    ui->checkBoxTeacher->setChecked(false);
}

void AdminDialog::on_checkBoxTeacher_pressed()
{
    ui->checkBoxRuleMaker->setChecked(false);
    ui->checkBoxAdmin->setChecked(false);
}


void AdminDialog::on_checkBoxRuleMaker_pressed()
{
    ui->checkBoxTeacher->setChecked(false);
    ui->checkBoxAdmin->setChecked(false);
}

