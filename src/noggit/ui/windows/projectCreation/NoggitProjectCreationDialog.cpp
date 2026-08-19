#include <noggit/ui/windows/projectCreation/NoggitProjectCreationDialog.h>
#include <ui_NoggitProjectCreationDialog.h>
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>

#include <filesystem>

NoggitProjectCreationDialog::NoggitProjectCreationDialog(ProjectInformation& project_information, QWidget* parent)
    : QDialog(parent)
    , ui(new ::Ui::NoggitProjectCreationDialog)
    , _project_information(project_information)
{
  setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

  ui->setupUi(this);

  QIcon icon = QIcon(":/icon-wrath");
  ui->expansion_icon->setPixmap(icon.pixmap(QSize(32, 32)));
  ui->expansion_icon->setObjectName("icon");
  ui->expansion_icon->setStyleSheet("QLabel#icon { padding: 0px }");

  QObject::connect(ui->project_expansion, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int index)
                   {
                     auto version_selected = ui->project_expansion->currentText().toStdString();

                     QIcon icon;
                     if (version_selected == "Wrath Of The Lich King")
                       icon = QIcon(":/icon-wrath");
                     else if (version_selected == "Shadowlands")
                       icon = QIcon(":/icon-shadow");
                      
                     ui->expansion_icon->setPixmap(icon.pixmap(QSize(32, 32)));
                   }
  );

  QObject::connect(ui->clientPathField_browse, &QPushButton::clicked, [this]
                   {
                     // TODO: implement automatic client path detection
                     QSettings settings;
                     auto default_path = settings.value("project/game_path").toString();
                     ui->clientPathField->setText(default_path);

                     QString folder_name = QFileDialog::getExistingDirectory(this, "Select Client Directory", default_path,
                                                                             QFileDialog::ShowDirsOnly |
                                                                             QFileDialog::DontResolveSymlinks);
                     ui->clientPathField->setText(folder_name);
                   }
  );

  QObject::connect(ui->projectPathField_browse, &QPushButton::clicked, [this]
                   {
                     QString folder_name = QFileDialog::getExistingDirectory(this, "Select Project Directory", "/",
                                                                             QFileDialog::ShowDirsOnly |
                                                                             QFileDialog::DontResolveSymlinks);
                     ui->projectPathField->setText(folder_name);
                   }
  );

  QObject::connect(ui->button_ok, &QPushButton::clicked, [&]
                   {
                     project_information.project_name = ui->projectName->text().toStdString();

                     if (project_information.project_name.empty())
                     {
                       QMessageBox::critical(this, "Error", "Project must have a name.");
                       return;
                     }

                     project_information.game_client_path = ui->clientPathField->text().toStdString();

                     if (project_information.game_client_path.empty())
                     {
                       QMessageBox::critical(this, "Error", "Game client path is empty.");
                       return;
                     }

                     std::filesystem::path game_path(project_information.game_client_path);
                     if (!std::filesystem::exists(game_path))
                     {
                       QMessageBox::critical(this, "Error", "Game client path does not exist. \nAvoid special characters.");
                       return;
                     }

                     project_information.project_path = ui->projectPathField->text().toStdString();

                     std::filesystem::path project_path(project_information.project_path);

                     if (project_path.empty())
                     {
                       QMessageBox::critical(this, "Error", "Project path is empty.");
                       return;
                     }

                     if (!std::filesystem::exists(project_path))
                     {
                       QMessageBox::critical(this, "Error", "Project path does not exist. \nAvoid special characters.");
                       return;
                     }

                     if (project_path == project_path.root_path())
                     {
                       QMessageBox::critical(this, "Error", "Project path can't be the root of a drive.\nPoint to a folder, preferrably empty.");
                       return;
                     }

                     project_information.game_client_version = ui->project_expansion->currentText().toStdString();

                     // 9.1.5x: sanity checks and bootstrap for modern (CASC based) clients.
                     if (project_information.game_client_version == "Shadowlands")
                     {
                       std::filesystem::path const build_info = game_path / ".build.info";
                       std::filesystem::path const retail_build_info = game_path / "_retail_" / ".build.info";

                       if (!std::filesystem::exists(build_info) && !std::filesystem::exists(retail_build_info))
                       {
                         QMessageBox::critical(this, "Error"
                           , "The game client path does not look like a modern (CASC) client folder, "
                             "no '.build.info' file was found.\n\n"
                             "For a Battle.net install, point to the raw install folder "
                             "(or its '_retail_' sub folder).");
                         return;
                       }

                       if (std::filesystem::exists(retail_build_info) && !std::filesystem::exists(build_info))
                       {
                         QMessageBox::information(this, "Note"
                           , "The client was found in the '_retail_' sub folder.\n"
                             "Consider pointing the game client path to it directly, "
                             "some client setups require it.");
                       }

                       // modern clients address files by FileDataID, so reading the client data
                       // strictly requires a listfile.csv in the project folder.
                       std::filesystem::path const listfile_target = project_path / "listfile.csv";

                       if (!std::filesystem::exists(listfile_target))
                       {
                         bool copied = false;

                         if (QMessageBox::question(this, "listfile.csv required"
                             , "Shadowlands (CASC) projects require the community \"listfile.csv\" "
                               "(FileDataID to file path mapping) in the project folder.\n\n"
                               "Select one now to copy it into the project?",
                             QMessageBox::Yes | QMessageBox::No
                             , QMessageBox::Yes) == QMessageBox::Yes)
                         {
                           QString source = QFileDialog::getOpenFileName(this
                             , "Select listfile.csv", QString(), "Listfile (*.csv);;All files (*)");

                           if (!source.isEmpty())
                           {
                             std::error_code ec;
                             std::filesystem::copy_file(std::filesystem::path(source.toStdString())
                               , listfile_target
                               , std::filesystem::copy_options::overwrite_existing
                               , ec);

                             if (ec)
                             {
                               QMessageBox::critical(this, "Error"
                                 , std::string("Failed to copy the listfile into the project folder:\n"
                                     + ec.message()).c_str());
                               return;
                             }

                             copied = true;
                           }
                         }

                         if (!copied)
                         {
                           QMessageBox::warning(this, "Missing listfile"
                             , "The project was created without listfile.csv.\n"
                               "Noggit will not be able to read the client data until you copy a "
                               "listfile.csv into the project folder (you can still add it later, "
                               "e.g. from wago.tools / wow.tools).");
                         }
                       }
                     }

                     done(QDialog::Accepted);
                     close();
                   }
  );

  QObject::connect(ui->button_cancel, &QPushButton::clicked, [&]
                   {
                     done(QDialog::Rejected);
                     close();
                   }
  );
}

NoggitProjectCreationDialog::~NoggitProjectCreationDialog()
{
  delete ui;
}
