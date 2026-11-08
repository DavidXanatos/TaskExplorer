; English is default language and can be translated to any
; of the languages listed in the languages section.
; The translations can be added to the CustomMessages section.
; The content of this script will be included by TaskExplorer.iss,
; so please edit with care.


[Languages]

Name: "English";             MessagesFile: "compiler:Default.isl"
Name: "ChineseSimplified";   MessagesFile: "isl\ChineseSimplified.isl"
Name: "ChineseTraditional";  MessagesFile: "isl\ChineseTraditional.isl"
Name: "Dutch";               MessagesFile: "compiler:Languages\Dutch.isl"
Name: "French";              MessagesFile: "compiler:Languages\French.isl"
Name: "German";              MessagesFile: "compiler:Languages\German.isl"
Name: "Hungarian";           MessagesFile: "compiler:Languages\Hungarian.isl"
Name: "Italian";             MessagesFile: "compiler:Languages\Italian.isl"
Name: "Korean";              MessagesFile: "isl\Korean.isl"
Name: "Polish";              MessagesFile: "compiler:Languages\Polish.isl"
Name: "BrazilianPortuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl
Name: "Portuguese";          MessagesFile: "compiler:Languages\Portuguese.isl"
Name: "Russian";             MessagesFile: "compiler:Languages\Russian.isl"
Name: "Spanish";             MessagesFile: "compiler:Languages\Spanish.isl"
Name: "Swedish";             MessagesFile: "isl\Swedish.isl"
Name: "Turkish";             MessagesFile: "compiler:Languages\Turkish.isl"
Name: "Ukrainian";           MessagesFile: "compiler:Languages\Ukrainian.isl"
Name: "Vietnamese";          MessagesFile: "isl\Vietnamese.isl"
;Name: "japanese";           MessagesFile: "compiler:Languages\Japanese.isl"
;Name: "armenian";           MessagesFile: "compiler:Languages\Armenian.isl"
;Name: "bulgarian";          MessagesFile: "compiler:Languages\Bulgarian.isl"
;Name: "catalan";            MessagesFile: "compiler:Languages\Catalan.isl"
;Name: "corsican";           MessagesFile: "compiler:Languages\Corsican.isl"
;Name: "czech";              MessagesFile: "compiler:Languages\Czech.isl"
;Name: "danish";             MessagesFile: "compiler:Languages\Danish.isl"
;Name: "finnish";            MessagesFile: "compiler:Languages\Finnish.isl"
;Name: "hebrew";             MessagesFile: "compiler:Languages\Hebrew.isl"
;Name: "icelandic";          MessagesFile: "compiler:Languages\Icelandic.isl"
;Name: "norwegian";          MessagesFile: "compiler:Languages\Norwegian.isl"
;Name: "slovak";             MessagesFile: "compiler:Languages\Slovak.isl"
;Name: "slovenian";          MessagesFile: "compiler:Languages\Slovenian.isl"



[CustomMessages]

; English
English.CustomPageLabel1=Select Installation Type
English.CustomPageLabel2=How should be installed
English.CustomPageLabel3=Choose the installation mode
English.CustomPageInstallMode=Install {#MyAppName} on this computer
English.CustomPageUpgradeMode=Update existing {#MyAppName} installation
English.CustomPagePortableMode=Extract all files to a directory for portable use
English.RequiresWin7OrLater={#MyAppName} requires Windows 7 or later.
English.StartProgram=Start {#MyAppName}
English.LaunchProgram=Launch %1...
English.VisitOurWebsite=Visit Our Website!

; Chinese Simplified
ChineseSimplified.CustomPageLabel1=选择安装方式
ChineseSimplified.CustomPageLabel2=应当如何安装
ChineseSimplified.CustomPageLabel3=选择安装模式
ChineseSimplified.CustomPageInstallMode=在这台计算机上安装 {#MyAppName}
ChineseSimplified.CustomPageUpgradeMode=更新当前已安装的 {#MyAppName}
ChineseSimplified.CustomPagePortableMode=提取全部文件到指定目录供便携化使用
ChineseSimplified.RequiresWin7OrLater={#MyAppName} 需要 Windows 7 或更高版本。

; Chinese Traditional
ChineseTraditional.CustomPageLabel1=選擇安裝方式
ChineseTraditional.CustomPageLabel2=應該如何安裝
ChineseTraditional.CustomPageLabel3=選擇安裝模式
ChineseTraditional.CustomPageInstallMode=在這台電腦上安裝 {#MyAppName}
ChineseTraditional.CustomPageUpgradeMode=更新目前已安裝的 {#MyAppName}
ChineseTraditional.CustomPagePortableMode=解壓縮所有檔案到指定目錄供便攜化使用
ChineseTraditional.RequiresWin7OrLater={#MyAppName} 需要 Windows 7 或更高版本。

; Dutch
Dutch.CustomPageLabel1=Installatietype selecteren
Dutch.CustomPageLabel2=Hoe moet er geïnstalleerd worden
Dutch.CustomPageLabel3=Installatiemodus kiezen
Dutch.CustomPageInstallMode={#MyAppName} op deze computer installeren
Dutch.CustomPageUpgradeMode=Bestaande installatie van {#MyAppName} bijwerken
Dutch.CustomPagePortableMode=Alle bestanden naar een map uitpakken voor draagbaar gebruik
Dutch.RequiresWin7OrLater={#MyAppName} vereist Windows 7 of later.

; French
French.CustomPageLabel1=Sélectionner le type d'installation
French.CustomPageLabel2=Comment doit-on l'installer ?
French.CustomPageLabel3=Choisir le mode d'installation
French.CustomPageInstallMode=Installer {#MyAppName} sur cet ordinateur
French.CustomPageUpgradeMode=Mettre à jour l'installation de {#MyAppName} existante
French.CustomPagePortableMode=Extraire tous les fichiers dans un répertoire pour une utilisation portable
French.RequiresWin7OrLater={#MyAppName} nécessite Windows 7 ou une version ultérieure.

; German.
German.CustomPageLabel1=Wählen Sie die Installationsart aus
German.CustomPageLabel2=Wie soll installiert werden
German.CustomPageLabel3=Wählen Sie die Installationsmethode
German.CustomPageInstallMode=Installiere {#MyAppName} auf diesem Computer
German.CustomPageUpgradeMode=Update der bestehenden {#MyAppName} Installation
German.CustomPagePortableMode=Extrahiere alle Dateien in einen Ordner zur tragbaren Verwendung
German.RequiresWin7OrLater={#MyAppName} benötigt Windows 7 oder neuer.

; Hungarian
Hungarian.CustomPageLabel1=Válassza ki a telepítés típusát
Hungarian.CustomPageLabel2=Hogyan kell telepíteni?
Hungarian.CustomPageLabel3=Válassza ki a telepítési módot
Hungarian.CustomPageInstallMode=Telepítse a {#MyAppName} alkalmazást erre a számítógépre
Hungarian.CustomPageUpgradeMode=Frissítse a meglévő {#MyAppName} telepítést
Hungarian.CustomPagePortableMode=Csomagolja ki az összes fájlt egy könyvtárba hordozható használatra
Hungarian.RequiresWin7OrLater={#MyAppName} alkalmazáshoz Windows 7 vagy újabb rendszer szükséges.

; Italian
Italian.CustomPageLabel1=Seleziona tipo di installazione
Italian.CustomPageLabel2=Come dovrebbe essere installato
Italian.CustomPageLabel3=Scegli la modalità di installazione
Italian.CustomPageInstallMode=Installa {#MyAppName} in questo computer
Italian.CustomPageUpgradeMode=Aggiorna l'installazione di {#MyAppName} esistente
Italian.CustomPagePortableMode=Estrai tutti i file in una cartella per uso portatile
Italian.RequiresWin7OrLater={#MyAppName} richiede Windows 7 o superiore.
English.VisitOurWebsite=Visita il nostro sito web!
Italian.StartProgram=Esegui {#MyAppName}
Italian.AdditionalIcons=Collegamenti:
Italian.CreateDesktopIcon=Crea collegamento programma sul &desktop
Italian.CreateQuickLaunchIcon=Crea collegamento programma nella &barra 'Avvio veloce'
Italian.AssocFileExtension=&Associa i file con estensione '%2' a '%1'
Italian.AssocingFileExtension=Associazione file con estensione '%2' a '%1'...
Italian.LaunchProgram=Esegui %1...

; Korean
Korean.CustomPageLabel1=설치 유형 선택
Korean.CustomPageLabel2=설치 방법
Korean.CustomPageLabel3=설치 모드 선택
Korean.CustomPageInstallMode=이 컴퓨터에 {#MyAppName} 설치
Korean.CustomPageUpgradeMode=기존 {#MyAppName} 설치 업데이트
Korean.CustomPagePortableMode=휴대용 사용을 위해 모든 파일을 디렉터리에 추출
Korean.RequiresWin7OrLater={#MyAppName}를 사용하려면 Windows 7 이상이 필요합니다.

; Polish
Polish.CustomPageLabel1=Wybierz typ instalacji
Polish.CustomPageLabel2=Jak należy instalować
Polish.CustomPageLabel3=Wybierz tryb instalacji
Polish.CustomPageInstallMode=Zainstaluj {#MyAppName} na tym komputerze
Polish.CustomPageUpgradeMode=Aktualizacja istniejącej instalacji {#MyAppName}
Polish.CustomPagePortableMode=Wypakuj wszystkie pliki do katalogu, aby użyć trybu przenośnego
Polish.RequiresWin7OrLater={#MyAppName} wymaga systemu Windows 7 lub nowszego.

; Brazilian Portuguese
BrazilianpPrtuguese.CustomPageLabel1=Selecione Tipo de Instalação
BrazilianpPrtuguese.CustomPageLabel2=Como deve ser instalado
BrazilianpPrtuguese.CustomPageLabel3=Escolha o modo de instalação
BrazilianpPrtuguese.CustomPageInstallMode=Instalar {#MyAppName} neste computador
BrazilianpPrtuguese.CustomPageUpgradeMode=Atualizar instalação existente do {#MyAppName}
BrazilianpPrtuguese.CustomPagePortableMode=Extrair todos os arquivos para um diretório para uso portable
BrazilianpPrtuguese.RequiresWin7OrLater={#MyAppName} requer o Windows 7 ou posterior.

; Portuguese
Portuguese.CustomPageLabel1=Selecione Tipo de Instalação
Portuguese.CustomPageLabel2=Como deve ser instalado
Portuguese.CustomPageLabel3=Escolha o modo de instalação
Portuguese.CustomPageInstallMode=Instalar {#MyAppName} neste computador
Portuguese.CustomPageUpgradeMode=Atualizar instalação existente do {#MyAppName}
Portuguese.CustomPagePortableMode=Extrair todos os ficheiro para um diretório para uso portable
Portuguese.RequiresWin7OrLater={#MyAppName} requer o Windows 7 ou posterior.

; Russian
Russian.CustomPageLabel1=Выберите тип установки
Russian.CustomPageLabel2=Как следует установить
Russian.CustomPageLabel3=Выберите режим установки
Russian.CustomPageInstallMode=Установить {#MyAppName} на этом компьютере
Russian.CustomPageUpgradeMode=Обновить существующую установку {#MyAppName}
Russian.CustomPagePortableMode=Извлечь все файлы в каталог для портативного использования
Russian.RequiresWin7OrLater={#MyAppName} требует Windows 7 или новее.

; Spanish
Spanish.CustomPageLabel1=Seleccione el tipo de instalación
Spanish.CustomPageLabel2=¿Cómo se debe instalar?
Spanish.CustomPageLabel3=Elija el modo de instalación
Spanish.CustomPageInstallMode=Instalar {#MyAppName} en esta computadora
Spanish.CustomPageUpgradeMode=Actualizar la instalación existente de {#MyAppName}
Spanish.CustomPagePortableMode=Extraiga todos los archivos a un directorio para uso portátil
Spanish.RequiresWin7OrLater={#MyAppName} requiere Windows 7 o posterior.

; Swedish
Swedish.CustomPageLabel1=Välj installationstyp
Swedish.CustomPageLabel2=Hur ska installeringen ske
Swedish.CustomPageLabel3=Välj installationsläget
Swedish.CustomPageInstallMode=Installera {#MyAppName} på denna dator
Swedish.CustomPageUpgradeMode=Uppdatera existerande {#MyAppName} installation
Swedish.CustomPagePortableMode=Extrahera alla filer till en katalog för portabel användning
Swedish.RequiresWin7OrLater={#MyAppName} kräver Windows 7 eller senare.

; Turkish
Turkish.CustomPageLabel1=Kurulum Türünü Seçin
Turkish.CustomPageLabel2=Nasıl kurulacağını seçin
Turkish.CustomPageLabel3=Yükleme modunu seçin
Turkish.CustomPageInstallMode={#MyAppName} uygulamasını bu bilgisayara kur
Turkish.CustomPageUpgradeMode=Mevcut {#MyAppName} kurulumunu güncelle
Turkish.CustomPagePortableMode=Taşınabilir kullanım için tüm dosyaları bir dizine çıkar
Turkish.RequiresWin7OrLater={#MyAppName}, Windows 7 veya sonraki bir sürümünü gerektirir.

; Ukrainian
Ukrainian.CustomPageLabel1=Оберіть тип встановки
Ukrainian.CustomPageLabel2=Як потрібно встановити
Ukrainian.CustomPageLabel3=Оберіть режим встановки
Ukrainian.CustomPageInstallMode=Встановити {#MyAppName} на цьому пристрої
Ukrainian.CustomPageUpgradeMode=Оновити існуючу встановку {#MyAppName}
Ukrainian.CustomPagePortableMode=Витягти всі файли у каталог для портативного режиму
Ukrainian.RequiresWin7OrLater=Для {#MyAppName} потрібно Windows 7 або новіше.

; Vietnamese
Vietnamese.CustomPageLabel1=Chọn loại cài đặt
Vietnamese.CustomPageLabel2=Cài đặt như thế nào
Vietnamese.CustomPageLabel3=Chọn chế độ cài đặt
Vietnamese.CustomPageInstallMode=Cài đặt {#MyAppName} trên máy tính này
Vietnamese.CustomPageUpgradeMode=Cập nhật cài đặt hiện có của {#MyAppName}
Vietnamese.CustomPagePortableMode=Giải nén tất cả các tệp vào một thư mục để sử dụng di động
Vietnamese.RequiresWin7OrLater={#MyAppName} yêu cầu Windows 7 trở lên.

; Japanese
;Japanese.CustomPageLabel1=インストールタイプの選択
;Japanese.CustomPageLabel2=インストール方法
;Japanese.CustomPageLabel3=インストールモードの選択
;Japanese.CustomPageInstallMode=このコンピュータに {#MyAppName} をインストール
;Japanese.CustomPageUpgradeMode=既存の {#MyAppName} インストールを更新
;Japanese.CustomPagePortableMode=すべてのファイルをポータブル用にディレクトリに抽出
;Japanese.RequiresWin7OrLater={#MyAppName} には Windows 7 以降が必要です。
