# -*- coding: utf-8 -*-
"""Las traducciones de la interfaz (v1.07). Cada clave es el texto en español tal como esta en el
codigo; los valores, en ingles, frances, aleman, chino (simplificado), japones y ruso (este en traducciones_ru.py). `{}` marca donde va
un dato. Este archivo genera src/idioma_tabla.inc:  python3 tools/traducciones.py"""
T = {
"No volver a preguntar": ("Don't ask again", "Ne plus demander", "Nicht mehr fragen", "不再询问", "今後確認しない"),
"Plano de apoyo": ("Screen plane", "Plan d'appui", "Bildebene", "支撑平面", "基準面"),
"Profundidad": ("Depth", "Profondeur", "Tiefe", "深度", "奥行き"),
"Salida": ("Pop-out", "Jaillissement", "Pop-out", "出屏", "飛び出し"),
"Ctrl+Shift+  1 el 3D · 3/4 {} · 5/6 salida · F1 oculta · Esc sale": ("Ctrl+Shift+  1 3D · 3/4 {} · 5/6 pop-out · F1 hides · Esc exits", "Ctrl+Maj+  1 la 3D · 3/4 {} · 5/6 jaillissement · F1 masque · Échap quitte", "Strg+Umschalt+  1 3D · 3/4 {} · 5/6 Pop-out · F1 ausblenden · Esc beendet", "Ctrl+Shift+  1 开关3D · 3/4 {} · 5/6 出屏 · F1 隐藏 · Esc 退出", "Ctrl+Shift+  1 3D · 3/4 {} · 5/6 飛び出し · F1 隠す · Esc 終了"),
"plano": ("plane", "plan", "Ebene", "平面", "基準面"),
"profundidad": ("depth", "profondeur", "Tiefe", "深度", "奥行き"),
"Hay una versión nueva de OdysseyVK": ("A new version of OdysseyVK is available", "Une nouvelle version d'OdysseyVK est disponible", "Eine neue Version von OdysseyVK ist da", "OdysseyVK 有新版本", "OdysseyVK の新しいバージョンがあります"),
"OdysseyVK {} en GitHub · tienes la {}": ("OdysseyVK {} on GitHub · you have {}", "OdysseyVK {} sur GitHub · vous avez la {}", "OdysseyVK {} auf GitHub · installiert ist {}", "GitHub 上为 OdysseyVK {} · 当前为 {}", "GitHub に OdysseyVK {} · 現在 {}"),
"Se instala sola (pide tu clave) y el vídeo o juego que haya en 3D se reabre.": ("It installs itself (asks for your password) and reopens any video or game in 3D.", "Elle s'installe seule (mot de passe demandé) et rouvre la vidéo ou le jeu en 3D.", "Installiert sich selbst (fragt nach dem Passwort); Video oder Spiel in 3D öffnet sich wieder.", "自动安装（需要密码），正在 3D 中的视频或游戏会重新打开。", "自動でインストールされ（パスワードが必要）、3D 中の動画やゲームは開き直されます。"),
"Actualizar": ("Update", "Mettre à jour", "Aktualisieren", "更新", "更新"),
"Luego": ("Later", "Plus tard", "Später", "稍后", "後で"),
"No avisar de actualizaciones": ("Don't notify about updates", "Ne pas signaler les mises à jour", "Nicht über Updates informieren", "不再提示更新", "更新を通知しない"),
"Entendido": ("Got it", "Compris", "Verstanden", "知道了", "了解"),
"¿Qué tipo de vídeo estás viendo?": ("What kind of video are you watching?", "Quel type de vidéo regardez-vous ?", "Welche Art von Video siehst du?", "你在看哪种视频？", "どの種類の動画を見ていますか？"),
"Vídeo normal: una sola imagen; se convierte a 3D.": ("Normal video: a single image; it gets converted to 3D.", "Vidéo normale : une seule image, convertie en 3D.", "Normales Video: ein einzelnes Bild, wird in 3D umgewandelt.", "普通视频：单一画面，会被转换为 3D。", "通常の動画：1 枚の映像を 3D に変換します。"),
"Vídeo dividido: trae dos imágenes, una al lado de la otra.": ("Split video: two images, side by side.", "Vidéo divisée : deux images côte à côte.", "Geteiltes Video: zwei Bilder nebeneinander.", "分屏视频：左右并排的两幅画面。", "分割動画：左右に並んだ 2 枚の映像。"),
"Elige la opción correcta para la conversión 3D.": ("Choose the right option for the 3D conversion.", "Choisissez la bonne option pour la conversion 3D.", "Wähle die passende Option für die 3D-Umwandlung.", "请为 3D 转换选择正确的选项。", "3D 変換に合う方を選んでください。"),
"Vídeo normal": ("Normal video", "Vidéo normale", "Normales Video", "普通视频", "通常の動画"),
"Vídeo dividido": ("Split video", "Vidéo divisée", "Geteiltes Video", "分屏视频", "分割動画"),
"una sola imagen": ("a single image", "une seule image", "ein einzelnes Bild", "单一画面", "1 枚の映像"),
"dos imágenes lado a lado": ("two images side by side", "deux images côte à côte", "zwei Bilder nebeneinander", "左右两幅画面", "左右 2 枚の映像"),
"Guardar el tipo de vídeo y no volver a preguntar": ("Remember the video type and don't ask again", "Retenir le type de vidéo et ne plus demander", "Videotyp merken und nicht mehr fragen", "记住视频类型，不再询问", "動画の種類を保存して今後確認しない"),
"Hay una actualización pendiente: OdysseyVK {}. Pica para instalarla.": ("An update is pending: OdysseyVK {}. Click to install it.", "Mise à jour en attente : OdysseyVK {}. Cliquez pour l'installer.", "Update verfügbar: OdysseyVK {}. Zum Installieren klicken.", "有待安装的更新：OdysseyVK {}。点击安装。", "保留中の更新：OdysseyVK {}。クリックでインストール。"),
"OdysseyVK está al día. Pica para volver a comprobarlo.": ("OdysseyVK is up to date. Click to check again.", "OdysseyVK est à jour. Cliquez pour vérifier à nouveau.", "OdysseyVK ist aktuell. Klicken, um erneut zu prüfen.", "OdysseyVK 已是最新。点击重新检查。", "OdysseyVK は最新です。クリックで再確認。"),
"Aún no se ha buscado ninguna actualización. Pica para buscarla.": ("No update check yet. Click to check.", "Aucune vérification pour l'instant. Cliquez pour vérifier.", "Noch nicht nach Updates gesucht. Zum Suchen klicken.", "尚未检查更新。点击检查。", "まだ更新を確認していません。クリックで確認。"),
"Idioma": ("Language", "Langue", "Sprache", "语言", "言語"),
"Ajustes": ("Settings", "Réglages", "Einstellungen", "设置", "設定"),
"Juegos": ("Games", "Jeux", "Spiele", "游戏", "ゲーム"),
"OdysseyVK — Ajustes": ("OdysseyVK — Settings", "OdysseyVK — Réglages", "OdysseyVK — Einstellungen", "OdysseyVK — 设置", "OdysseyVK — 設定"),
"Clic: ajustes · clic central: juegos": ("Click: settings · middle click: games", "Clic : réglages · clic du milieu : jeux", "Klick: Einstellungen · Mittelklick: Spiele", "单击：设置 · 中键：游戏", "クリック：設定 · 中クリック：ゲーム"),
"vídeo normal": ("normal video", "vidéo normale", "normales Video", "普通视频", "通常の動画"),
"vídeo dividido": ("split video", "vidéo divisée", "geteiltes Video", "分屏视频", "分割動画"),
"arriba y abajo": ("top-bottom", "haut-bas", "oben-unten", "上下", "上下"),
"sin 3D": ("no 3D", "sans 3D", "ohne 3D", "无 3D", "3D なし"),
"vídeo dividido: el relieve del propio vídeo": ("split video: the video's own depth", "vidéo divisée : le relief de la vidéo", "geteiltes Video: die eigene Tiefe des Videos", "分屏视频：使用视频自身的立体感", "分割動画：動画そのものの立体感"),
"vídeo dividido suavizado: el relieve lo pone el conversor": ("smoothed split video: depth from the converter", "vidéo divisée adoucie : relief du convertisseur", "geglättetes geteiltes Video: Tiefe vom Konverter", "平滑分屏视频：立体感由转换器生成", "なめらかな分割動画：立体感は変換器が作成"),
"no se pudo guardar la elección": ("couldn't save the choice", "impossible d'enregistrer le choix", "Auswahl konnte nicht gespeichert werden", "无法保存选择", "選択を保存できませんでした"),
"{} pasa directo al 3D, sin preguntar": ("{} goes straight to 3D, without asking", "{} passe directement en 3D, sans demander", "{} wechselt direkt in 3D, ohne zu fragen", "{} 将直接进入 3D，不再询问", "{} は確認なしで直接 3D になります"),
"la próxima vez se vuelve a preguntar por {}": ("next time it will ask again for {}", "la prochaine fois, on redemandera pour {}", "beim nächsten Mal wird für {} wieder gefragt", "下次会再次询问 {}", "次回は {} について再度確認します"),
"el conversor de vídeo normal no abrió": ("the normal-video converter didn't start", "le convertisseur de vidéo normale n'a pas démarré", "der Konverter für normale Videos startete nicht", "普通视频转换器未能启动", "通常動画の変換器が起動しませんでした"),
"la otra red no abrió: seguimos con la anterior": ("the other network didn't load: keeping the previous one", "l'autre réseau n'a pas chargé : on garde le précédent", "das andere Netz lud nicht: das vorherige bleibt", "另一个网络未能加载：继续使用之前的", "もう一方のネットワークを読み込めず、前のものを使います"),
"cambiar de red toca cerrar y abrir el visor": ("switching network needs reopening the viewer", "changer de réseau impose de rouvrir la visionneuse", "Netzwechsel erfordert einen Neustart des Viewers", "切换网络需要重新打开查看器", "ネットワークの切り替えにはビューアの再起動が必要です"),
"esa red no existe: {}": ("that network doesn't exist: {}", "ce réseau n'existe pas : {}", "dieses Netz existiert nicht: {}", "该网络不存在：{}", "そのネットワークは存在しません：{}"),
"la red pedida no abrió: seguimos con la anterior": ("the requested network didn't load: keeping the previous one", "le réseau demandé n'a pas chargé : on garde le précédent", "das angeforderte Netz lud nicht: das vorherige bleibt", "所请求的网络未能加载：继续使用之前的", "要求したネットワークを読み込めず、前のものを使います"),
"la lente 3D no respondió": ("the 3D lens didn't respond", "la lentille 3D n'a pas répondu", "die 3D-Linse antwortete nicht", "3D 透镜无响应", "3D レンズが応答しません"),
"el scaler del monitor no respondió": ("the monitor's scaler didn't respond", "le scaler de l'écran n'a pas répondu", "der Scaler des Monitors antwortete nicht", "显示器缩放器无响应", "モニターのスケーラーが応答しません"),
"sin cámara: la trama va con la posición de fábrica": ("no camera: weaving uses the factory position", "pas de caméra : position d'usine", "keine Kamera: Werksposition", "无摄像头：使用出厂位置", "カメラなし：工場出荷時の位置で表示"),
"RED DE PROFUNDIDAD": ("DEPTH NETWORK", "RÉSEAU DE PROFONDEUR", "TIEFENNETZ", "深度网络", "深度ネットワーク"),
"FALTA LA RED DE PROFUNDIDAD": ("DEPTH NETWORK MISSING", "RÉSEAU DE PROFONDEUR MANQUANT", "TIEFENNETZ FEHLT", "缺少深度网络", "深度ネットワークがありません"),
"Sin ella no hay datos de profundidad: el vídeo normal no se puede pasar a 3D (el dividido sí).": ("Without it there is no depth data: normal video can't be turned into 3D (split video can).", "Sans lui, pas de données de profondeur : la vidéo normale ne passe pas en 3D (la divisée oui).", "Ohne es keine Tiefendaten: normale Videos können nicht in 3D (geteilte schon).", "没有它就没有深度数据：普通视频无法转为 3D（分屏视频可以）。", "これがないと深度データがなく、通常の動画を 3D にできません（分割動画は可）。"),
"Depth Anything V2 pequeña, 50 MB, de huggingface.co/onnx-community/depth-anything-v2-small; queda en ~/.local/share/odysseyvk/modelos.": ("Depth Anything V2 Small, 50 MB, from huggingface.co/onnx-community/depth-anything-v2-small; saved in ~/.local/share/odysseyvk/modelos.", "Depth Anything V2 Small, 50 Mo, depuis huggingface.co/onnx-community/depth-anything-v2-small ; dans ~/.local/share/odysseyvk/modelos.", "Depth Anything V2 Small, 50 MB, von huggingface.co/onnx-community/depth-anything-v2-small; in ~/.local/share/odysseyvk/modelos.", "Depth Anything V2 Small，50 MB，来自 huggingface.co/onnx-community/depth-anything-v2-small；保存在 ~/.local/share/odysseyvk/modelos。", "Depth Anything V2 Small（50 MB）、huggingface.co/onnx-community/depth-anything-v2-small から ~/.local/share/odysseyvk/modelos へ。"),
"Red de profundidad": ("Depth network", "Réseau de profondeur", "Tiefennetz", "深度网络", "深度ネットワーク"),
"Descargar la red para tener profundidad": ("Download the network to get depth", "Télécharger le réseau pour la profondeur", "Netz herunterladen, um Tiefe zu erhalten", "下载网络以获得深度", "深度を得るためネットワークをダウンロード"),
"Bajando…": ("Downloading…", "Téléchargement…", "Lädt…", "下载中…", "ダウンロード中…"),
"Lista": ("Ready", "Prêt", "Bereit", "就绪", "準備完了"),
"Descargar (50 MB)": ("Download (50 MB)", "Télécharger (50 Mo)", "Herunterladen (50 MB)", "下载（50 MB）", "ダウンロード（50 MB）"),
"descargando… {} de 50 MB": ("downloading… {} of 50 MB", "téléchargement… {} sur 50 Mo", "lädt… {} von 50 MB", "下载中… {} / 50 MB", "ダウンロード中… {} / 50 MB"),
"lista: la red abierta ya está en su carpeta": ("done: the open network is in its folder", "terminé : le réseau ouvert est dans son dossier", "fertig: das offene Netz liegt in seinem Ordner", "完成：开放网络已放入其文件夹", "完了：オープンネットワークを保存しました"),
"falta `curl`: instálalo y vuelve a intentarlo": ("`curl` is missing: install it and try again", "`curl` manquant : installez-le et réessayez", "`curl` fehlt: installieren und erneut versuchen", "缺少 `curl`：请安装后重试", "`curl` がありません。インストールして再試行してください"),
"la descarga falló (¿hay conexión?): vuelve a intentarlo": ("the download failed (connection?): try again", "le téléchargement a échoué (connexion ?) : réessayez", "Download fehlgeschlagen (Verbindung?): erneut versuchen", "下载失败（网络连接？）：请重试", "ダウンロードに失敗しました（接続？）。再試行してください"),
"el archivo bajó incompleto: vuelve a intentarlo": ("the file came down incomplete: try again", "le fichier est incomplet : réessayez", "die Datei ist unvollständig: erneut versuchen", "文件不完整：请重试", "ファイルが不完全です。再試行してください"),
"VERSIÓN": ("VERSION", "VERSION", "VERSION", "版本", "バージョン"),
"Instalada": ("Installed", "Installée", "Installiert", "已安装", "インストール済み"),
"En GitHub": ("On GitHub", "Sur GitHub", "Auf GitHub", "GitHub 上", "GitHub"),
"comprobando…": ("checking…", "vérification…", "prüfe…", "检查中…", "確認中…"),
"sin comprobar todavía": ("not checked yet", "pas encore vérifié", "noch nicht geprüft", "尚未检查", "未確認"),
"OdysseyVK {} · hay una versión nueva (visto a las {})": ("OdysseyVK {} · new version available (checked at {})", "OdysseyVK {} · nouvelle version disponible (vérifié à {})", "OdysseyVK {} · neue Version verfügbar (geprüft um {})", "OdysseyVK {} · 有新版本（检查于 {}）", "OdysseyVK {} · 新しいバージョンあり（{} に確認）"),
"OdysseyVK {} · estás al día (visto a las {})": ("OdysseyVK {} · up to date (checked at {})", "OdysseyVK {} · à jour (vérifié à {})", "OdysseyVK {} · aktuell (geprüft um {})", "OdysseyVK {} · 已是最新（检查于 {}）", "OdysseyVK {} · 最新です（{} に確認）"),
"Actualizar a OdysseyVK {}": ("Update to OdysseyVK {}", "Mettre à jour vers OdysseyVK {}", "Auf OdysseyVK {} aktualisieren", "更新到 OdysseyVK {}", "OdysseyVK {} に更新"),
"Buscar actualizaciones": ("Check for updates", "Rechercher des mises à jour", "Nach Updates suchen", "检查更新", "更新を確認"),
"Se baja de GitHub, se compila, se instala (pide tu clave) y el servicio se reinicia; el vídeo o juego en 3D se reabre.": ("Downloads from GitHub, builds, installs (asks for your password) and restarts the service; the 3D video or game reopens.", "Téléchargée depuis GitHub, compilée et installée (mot de passe demandé) ; le service redémarre et la 3D se rouvre.", "Lädt von GitHub, kompiliert, installiert (fragt nach dem Passwort), startet den Dienst neu; 3D öffnet sich wieder.", "从 GitHub 下载、编译、安装（需要密码）并重启服务；3D 中的视频或游戏会重新打开。", "GitHub から取得・ビルド・インストール（パスワードが必要）し、サービスを再起動。3D は開き直されます。"),
"Mira el repositorio {}": ("Checks the repository {}", "Consulte le dépôt {}", "Prüft das Repository {}", "检查仓库 {}", "リポジトリ {} を確認"),
"Comprobando…": ("Checking…", "Vérification…", "Prüfe…", "检查中…", "確認中…"),
"Buscar": ("Check", "Vérifier", "Suchen", "检查", "確認"),
"Avisar cuando haya una versión nueva": ("Notify when there is a new version", "Prévenir quand une nouvelle version sort", "Bei neuer Version benachrichtigen", "有新版本时提醒", "新しいバージョンを通知"),
"Una tarjeta en la orilla de la pantalla con el botón de actualizar; sin esto, solo el botón junto al aspa": ("A card at the screen edge with an update button; otherwise only the button next to the close cross", "Une carte au bord de l'écran avec le bouton de mise à jour ; sinon, le bouton près de la croix", "Eine Karte am Bildschirmrand mit Update-Knopf; sonst nur der Knopf neben dem Kreuz", "在屏幕边缘显示带更新按钮的卡片；关闭时仅显示关闭按钮旁的按钮", "画面の端に更新ボタン付きのカード。オフなら閉じるボタン横のボタンのみ"),
"Actualizar automáticamente": ("Update automatically", "Mettre à jour automatiquement", "Automatisch aktualisieren", "自动更新", "自動更新"),
"En cuanto haya una versión nueva se instala sola (igual pide tu clave para instalar)": ("As soon as there is a new version it installs itself (it still asks for your password)", "Dès qu'une nouvelle version sort, elle s'installe seule (mot de passe toujours demandé)", "Neue Versionen installieren sich sofort selbst (Passwort wird trotzdem abgefragt)", "有新版本时自动安装（仍需输入密码）", "新しいバージョンが出たら自動でインストール（パスワードは必要）"),
"OTROS": ("OTHER", "AUTRES", "SONSTIGES", "其他", "その他"),
"Mostrar OdysseyVK en la barra de tareas": ("Show OdysseyVK in the taskbar", "Afficher OdysseyVK dans la barre des tâches", "OdysseyVK in der Taskleiste anzeigen", "在任务栏显示 OdysseyVK", "タスクバーに OdysseyVK を表示"),
"En el gestor de tareas del panel de abajo: el visor 3D y estos ajustes mientras están abiertos": ("In the bottom panel's task manager: the 3D viewer and these settings while open", "Dans le gestionnaire de tâches du panneau du bas : la visionneuse 3D et ces réglages", "In der Fensterliste unten: der 3D-Viewer und diese Einstellungen, solange offen", "在下方面板的任务管理器中：3D 查看器和打开时的本设置", "下部パネルのタスクマネージャーに：3D ビューアと開いている間のこの設定"),
"Mostrar OdysseyVK en la bandeja del sistema": ("Show OdysseyVK in the system tray", "Afficher OdysseyVK dans la zone de notification", "OdysseyVK im Systemabschnitt anzeigen", "在系统托盘显示 OdysseyVK", "システムトレイに OdysseyVK を表示"),
"El icono junto al reloj y las notificaciones: clic abre estos ajustes, clic central los juegos": ("The icon next to the clock: click opens these settings, middle click the games", "L'icône près de l'horloge : clic pour ces réglages, clic du milieu pour les jeux", "Das Symbol neben der Uhr: Klick öffnet diese Einstellungen, Mittelklick die Spiele", "时钟旁的图标：单击打开本设置，中键打开游戏", "時計の横のアイコン：クリックでこの設定、中クリックでゲーム"),
"DESINSTALAR": ("UNINSTALL", "DÉSINSTALLER", "DEINSTALLIEREN", "卸载", "アンインストール"),
"Desinstalar OdysseyVK": ("Uninstall OdysseyVK", "Désinstaller OdysseyVK", "OdysseyVK deinstallieren", "卸载 OdysseyVK", "OdysseyVK をアンインストール"),
"Quita el programa, el servicio y la red de profundidad (pide tu clave); tu configuración se queda.": ("Removes the program, the service and the depth network (asks for your password); your settings stay.", "Supprime le programme, le service et le réseau (mot de passe demandé) ; vos réglages restent.", "Entfernt Programm, Dienst und Tiefennetz (fragt nach dem Passwort); Einstellungen bleiben.", "移除程序、服务和深度网络（需要密码）；你的配置会保留。", "プログラム、サービス、深度ネットワークを削除（パスワードが必要）。設定は残ります。"),
"¿Seguro? Pica otra vez": ("Sure? Click again", "Sûr ? Cliquez encore", "Sicher? Nochmal klicken", "确定？再点一次", "本当に？もう一度"),
"Desinstalar": ("Uninstall", "Désinstaller", "Deinstallieren", "卸载", "アンインストール"),
"VÍDEO": ("VIDEO", "VIDÉO", "VIDEO", "视频", "動画"),
"Suavizar los vídeos divididos": ("Smooth split videos", "Adoucir les vidéos divisées", "Geteilte Videos glätten", "平滑分屏视频", "分割動画をなめらかに"),
"Siempre": ("Always", "Toujours", "Immer", "总是", "常に"),
"Si hace falta": ("When needed", "Si besoin", "Bei Bedarf", "需要时", "必要なとき"),
"Nunca": ("Never", "Jamais", "Nie", "从不", "しない"),
"Fuerza del relieve": ("Depth strength", "Force du relief", "Tiefenstärke", "立体强度", "立体感の強さ"),
"Para el vídeo normal y el dividido suavizado · desde el siguiente vídeo": ("For normal and smoothed split video · from the next video", "Vidéo normale et divisée adoucie · dès la prochaine vidéo", "Für normale und geglättete Videos · ab dem nächsten Video", "用于普通视频和平滑分屏视频 · 从下一个视频起", "通常動画となめらかな分割動画 · 次の動画から"),
"PANTALLA": ("DISPLAY", "ÉCRAN", "BILDSCHIRM", "显示", "画面"),
"Reducir el fantasma": ("Reduce ghosting", "Réduire les images fantômes", "Geisterbilder reduzieren", "减少重影", "ゴーストを減らす"),
"Cuánta imagen del otro ojo se resta · se nota al momento": ("How much of the other eye's image is subtracted · applies instantly", "Part de l'image de l'autre œil soustraite · effet immédiat", "Wie viel vom anderen Auge abgezogen wird · wirkt sofort", "减去另一只眼图像的程度 · 立即生效", "反対の目の映像をどれだけ差し引くか · すぐに反映"),
"Seguir la cabeza con la cámara": ("Track your head with the camera", "Suivre la tête avec la caméra", "Kopf mit der Kamera verfolgen", "用摄像头追踪头部", "カメラで頭を追跡"),
"Desde el siguiente vídeo": ("From the next video", "Dès la prochaine vidéo", "Ab dem nächsten Video", "从下一个视频起", "次の動画から"),
"Atajos de teclado en todo el escritorio": ("Keyboard shortcuts across the desktop", "Raccourcis clavier sur tout le bureau", "Tastenkürzel im ganzen Desktop", "在整个桌面使用快捷键", "デスクトップ全体でショートカット"),
"APLICACIONES QUE NO PREGUNTAN": ("APPS THAT DON'T ASK", "APPLIS QUI NE DEMANDENT PAS", "APPS OHNE NACHFRAGE", "不再询问的应用", "確認しないアプリ"),
"Ninguna: se pregunta siempre (o se sigue lo de arriba).": ("None: it always asks (or follows the setting above).", "Aucune : on demande toujours (ou on suit le réglage ci-dessus).", "Keine: es wird immer gefragt (oder die Einstellung oben gilt).", "无：总是询问（或按上面的设置）。", "なし：常に確認します（または上の設定に従います）。"),
"VISOR 3D": ("3D VIEWER", "VISIONNEUSE 3D", "3D-VIEWER", "3D 查看器", "3D ビューア"),
"No hay ningún vídeo ni juego en 3D ahora mismo.": ("No video or game is in 3D right now.", "Aucune vidéo ni aucun jeu en 3D pour le moment.", "Gerade läuft kein Video oder Spiel in 3D.", "当前没有视频或游戏处于 3D。", "現在 3D の動画やゲームはありません。"),
"Cuadros por segundo": ("Frames per second", "Images par seconde", "Bilder pro Sekunde", "帧率", "フレームレート"),
"Conversor de relieve": ("Depth converter", "Convertisseur de relief", "Tiefenkonverter", "立体转换器", "立体変換器"),
"puesto · {} ms por mapa": ("on · {} ms per map", "activé · {} ms par carte", "an · {} ms pro Karte", "开启 · 每张图 {} 毫秒", "オン · 1 マップ {} ms"),
"apagado": ("off", "désactivé", "aus", "关闭", "オフ"),
"Cámara": ("Camera", "Caméra", "Kamera", "摄像头", "カメラ"),
"sin abrir": ("not open", "non ouverte", "nicht geöffnet", "未打开", "未使用"),
"TARJETAS GRÁFICAS": ("GRAPHICS CARDS", "CARTES GRAPHIQUES", "GRAFIKKARTEN", "显卡", "グラフィックカード"),
"nvidia-smi no respondió: no hay datos de las tarjetas.": ("nvidia-smi didn't respond: no card data.", "nvidia-smi n'a pas répondu : pas de données.", "nvidia-smi antwortete nicht: keine Kartendaten.", "nvidia-smi 无响应：没有显卡数据。", "nvidia-smi が応答せず、カード情報がありません。"),
"No encontré juegos instalados en las bibliotecas de Steam.": ("No installed games found in the Steam libraries.", "Aucun jeu installé dans les bibliothèques Steam.", "Keine installierten Spiele in den Steam-Bibliotheken.", "在 Steam 库中未找到已安装的游戏。", "Steam ライブラリにゲームがありません。"),
"3D: el juego entra solo en 3D al ponerse a pantalla completa. 2D: se deja como está.": ("3D: the game enters 3D on its own when it goes fullscreen. 2D: it's left as is.", "3D : le jeu passe seul en 3D en plein écran. 2D : il reste tel quel.", "3D: Das Spiel wechselt im Vollbild selbst in 3D. 2D: bleibt wie es ist.", "3D：游戏全屏时自动进入 3D。2D：保持原样。", "3D：全画面になるとゲームが自動で 3D に。2D：そのまま。"),
"no se pudo guardar: {}": ("couldn't save: {}", "impossible d'enregistrer : {}", "konnte nicht speichern: {}", "无法保存：{}", "保存できませんでした：{}"),
"se preguntará con la tarjeta": ("it will ask with the card", "on demandera avec la carte", "es wird mit der Karte gefragt", "将用卡片询问", "カードで確認します"),
"desde el siguiente vídeo, sin preguntar": ("from the next video, without asking", "dès la prochaine vidéo, sans demander", "ab dem nächsten Video, ohne zu fragen", "从下一个视频起，不再询问", "次の動画から確認なしで"),
"los vídeos divididos se suavizan siempre": ("split videos are always smoothed", "les vidéos divisées sont toujours adoucies", "geteilte Videos werden immer geglättet", "分屏视频总是平滑", "分割動画を常になめらかに"),
"con su propio relieve mientras quepa; suavizado solo si algo sale doble": ("with their own depth while it fits; smoothed only if something doubles", "avec leur relief tant qu'il tient ; adoucies si quelque chose se dédouble", "mit eigener Tiefe, solange sie passt; geglättet nur bei Doppelbildern", "在合适时使用自身立体感；只有出现重影时才平滑", "収まる限り動画の立体感で。二重に見えるときだけなめらかに"),
"los vídeos divididos se ven siempre con su propio relieve": ("split videos always use their own depth", "les vidéos divisées gardent toujours leur relief", "geteilte Videos nutzen immer ihre eigene Tiefe", "分屏视频总是使用自身立体感", "分割動画は常に動画の立体感で"),
"se aplica desde el siguiente vídeo": ("applies from the next video", "s'applique dès la prochaine vidéo", "gilt ab dem nächsten Video", "从下一个视频起生效", "次の動画から適用"),
"se resta el {} % de la imagen del otro ojo": ("{} % of the other eye's image is subtracted", "{} % de l'image de l'autre œil est soustrait", "{} % des Bildes des anderen Auges werden abgezogen", "减去另一只眼图像的 {} %", "反対の目の映像を {} % 差し引きます"),
"fuerza del relieve {} · desde el siguiente vídeo": ("depth strength {} · from the next video", "force du relief {} · dès la prochaine vidéo", "Tiefenstärke {} · ab dem nächsten Video", "立体强度 {} · 从下一个视频起生效", "立体感の強さ {} · 次の動画から"),
"no se avisará de versiones nuevas; el botón junto al aspa las sigue marcando": ("new versions won't be announced; the button next to the cross still shows them", "les nouvelles versions ne seront plus signalées ; le bouton près de la croix les indique", "neue Versionen werden nicht gemeldet; der Knopf neben dem Kreuz zeigt sie weiter", "不再提示新版本；关闭按钮旁的按钮仍会标记", "新しいバージョンは通知しません。閉じるボタン横には表示されます"),
"se avisará con una tarjeta en la orilla de la pantalla": ("a card at the screen edge will announce them", "une carte au bord de l'écran les signalera", "eine Karte am Bildschirmrand meldet sie", "将在屏幕边缘用卡片提示", "画面の端のカードで通知します"),
"OdysseyVK ya no sale en la barra de tareas": ("OdysseyVK no longer shows in the taskbar", "OdysseyVK n'apparaît plus dans la barre des tâches", "OdysseyVK erscheint nicht mehr in der Taskleiste", "OdysseyVK 不再显示在任务栏", "OdysseyVK をタスクバーに表示しません"),
"OdysseyVK vuelve a salir en la barra de tareas": ("OdysseyVK shows in the taskbar again", "OdysseyVK réapparaît dans la barre des tâches", "OdysseyVK erscheint wieder in der Taskleiste", "OdysseyVK 重新显示在任务栏", "OdysseyVK をタスクバーに表示します"),
"sin icono en la bandeja del sistema": ("no icon in the system tray", "pas d'icône dans la zone de notification", "kein Symbol im Systemabschnitt", "系统托盘中不显示图标", "システムトレイにアイコンを表示しません"),
"con icono en la bandeja del sistema": ("icon in the system tray", "icône dans la zone de notification", "Symbol im Systemabschnitt", "系统托盘中显示图标", "システムトレイにアイコンを表示します"),
"las versiones nuevas ya no se instalan solas": ("new versions no longer install themselves", "les nouvelles versions ne s'installent plus seules", "neue Versionen installieren sich nicht mehr selbst", "新版本不再自动安装", "新しいバージョンを自動でインストールしません"),
"las versiones nuevas se instalarán solas": ("new versions will install themselves", "les nouvelles versions s'installeront seules", "neue Versionen installieren sich selbst", "新版本将自动安装", "新しいバージョンを自動でインストールします"),
"mirando la versión de GitHub…": ("checking the version on GitHub…", "vérification de la version sur GitHub…", "prüfe die Version auf GitHub…", "正在检查 GitHub 上的版本…", "GitHub のバージョンを確認中…"),
"actualizando: se compila, se pide tu clave para instalar y el servicio se reinicia": ("updating: it builds, asks for your password to install and restarts the service", "mise à jour : compilation, mot de passe pour installer, puis redémarrage du service", "aktualisiere: kompiliert, fragt nach dem Passwort und startet den Dienst neu", "正在更新：编译、需要密码安装并重启服务", "更新中：ビルドし、パスワードを求めてインストールし、サービスを再起動します"),
"¿seguro? pica otra vez en los próximos 12 segundos para desinstalar": ("sure? click again within 12 seconds to uninstall", "sûr ? recliquez dans les 12 secondes pour désinstaller", "sicher? innerhalb von 12 Sekunden erneut klicken", "确定吗？12 秒内再点一次即可卸载", "本当に？12 秒以内にもう一度クリックでアンインストール"),
"desinstalando OdysseyVK…": ("uninstalling OdysseyVK…", "désinstallation d'OdysseyVK…", "OdysseyVK wird deinstalliert…", "正在卸载 OdysseyVK…", "OdysseyVK をアンインストール中…"),
"bajando la red abierta (50 MB)…": ("downloading the open network (50 MB)…", "téléchargement du réseau ouvert (50 Mo)…", "lade das offene Netz (50 MB)…", "正在下载开放网络（50 MB）…", "オープンネットワークをダウンロード中（50 MB）…"),
"se volverá a preguntar por {}": ("it will ask again for {}", "on redemandera pour {}", "für {} wird wieder gefragt", "将再次询问 {}", "{} について再度確認します"),
"ese juego se queda en 2D": ("that game stays in 2D", "ce jeu reste en 2D", "dieses Spiel bleibt in 2D", "该游戏保持 2D", "このゲームは 2D のまま"),
"ese juego entra solo en 3D": ("that game enters 3D on its own", "ce jeu passe seul en 3D", "dieses Spiel wechselt selbst in 3D", "该游戏将自动进入 3D", "このゲームは自動で 3D に"),
"arrancando {}…": ("launching {}…", "lancement de {}…", "starte {}…", "正在启动 {}…", "{} を起動中…"),
"ese no es un juego de Steam": ("that is not a Steam game", "ce n'est pas un jeu Steam", "das ist kein Steam-Spiel", "这不是 Steam 游戏", "Steam のゲームではありません"),
"actualizando a OdysseyVK {}: se compila, se pide tu clave y el servicio se reinicia": ("updating to OdysseyVK {}: it builds, asks for your password and restarts the service", "mise à jour vers OdysseyVK {} : compilation, mot de passe, puis redémarrage du service", "aktualisiere auf OdysseyVK {}: kompiliert, fragt nach dem Passwort, startet den Dienst neu", "正在更新到 OdysseyVK {}：编译、需要密码并重启服务", "OdysseyVK {} に更新中：ビルドし、パスワードを求め、サービスを再起動します"),
"no se pudo mirar GitHub (¿hay conexión?)": ("couldn't reach GitHub (connection?)", "impossible de joindre GitHub (connexion ?)", "GitHub nicht erreichbar (Verbindung?)", "无法访问 GitHub（网络连接？）", "GitHub に接続できません（接続？）"),
"hay una versión nueva: OdysseyVK {}": ("a new version is available: OdysseyVK {}", "une nouvelle version est disponible : OdysseyVK {}", "eine neue Version ist verfügbar: OdysseyVK {}", "有新版本：OdysseyVK {}", "新しいバージョンがあります：OdysseyVK {}"),
"estás al día (OdysseyVK {})": ("you're up to date (OdysseyVK {})", "vous êtes à jour (OdysseyVK {})", "du bist aktuell (OdysseyVK {})", "已是最新（OdysseyVK {}）", "最新です（OdysseyVK {}）"),
"red descargada: el 3D de vídeo normal ya puede calcular la profundidad": ("network downloaded: normal-video 3D can now compute depth", "réseau téléchargé : la 3D des vidéos normales peut calculer la profondeur", "Netz heruntergeladen: 3D für normale Videos kann jetzt Tiefe berechnen", "网络已下载：普通视频的 3D 现在可以计算深度", "ダウンロード完了：通常動画の 3D で深度を計算できます"),
"El juego tapa el 3D": ("The game covers the 3D", "Le jeu masque la 3D", "Das Spiel verdeckt das 3D", "游戏遮住了 3D", "ゲームが 3D を覆っています"),
"A pantalla completa exclusiva el escritorio pone al juego sobre el visor.": ("In exclusive fullscreen the desktop puts the game above the viewer.", "En plein écran exclusif, le bureau place le jeu au-dessus de la visionneuse.", "Im exklusiven Vollbild legt der Desktop das Spiel über den Viewer.", "独占全屏时，桌面会把游戏置于查看器之上。", "排他的全画面ではデスクトップがゲームをビューアの上に置きます。"),
"Ponlo en ventana sin bordes (borderless): así el 3D se ve encima.": ("Set it to a borderless window: then the 3D shows on top.", "Passez-le en fenêtre sans bordure : la 3D s'affiche par-dessus.", "Stelle es auf randloses Fenster: dann liegt das 3D oben.", "请改为无边框窗口：这样 3D 就会显示在上层。", "ボーダーレスウィンドウにすると 3D が上に表示されます。"),
"Con gamescope igual: también tiene que ir sin bordes.": ("Same with gamescope: it also has to be borderless.", "Pareil avec gamescope : il doit aussi être sans bordure.", "Mit gamescope ebenso: auch randlos.", "gamescope 也一样：同样需要无边框。", "gamescope でも同じく、ボーダーレスにしてください。"),
"El juego se cerró a los {} s de entrar en 3D": ("The game closed {} s after entering 3D", "Le jeu s'est fermé {} s après le passage en 3D", "Das Spiel endete {} s nach dem Wechsel in 3D", "游戏在进入 3D {} 秒后关闭", "ゲームは 3D になって {} 秒後に終了しました"),
"Si se cayó, ábrelo en ventana sin bordes (borderless) y vuelve a probar.": ("If it crashed, open it in a borderless window and try again.", "S'il a planté, lancez-le en fenêtre sans bordure et réessayez.", "Falls es abstürzte: im randlosen Fenster starten und erneut versuchen.", "如果崩溃了，请改用无边框窗口后再试。", "クラッシュした場合はボーダーレスウィンドウで開き直してください。"),
"A pantalla completa exclusiva choca con el visor al cambiar de modo.": ("In exclusive fullscreen it clashes with the viewer when switching modes.", "En plein écran exclusif, il entre en conflit avec la visionneuse.", "Im exklusiven Vollbild kollidiert es beim Moduswechsel mit dem Viewer.", "独占全屏在切换模式时会与查看器冲突。", "排他的全画面ではモード切替時にビューアと衝突します。"),
"Para dejarlo en 2D, cambia su interruptor en la pestaña Juegos.": ("To keep it in 2D, flip its switch in the Games tab.", "Pour le laisser en 2D, changez son interrupteur dans l'onglet Jeux.", "Für 2D den Schalter im Tab Spiele umstellen.", "如需保持 2D，请在“游戏”页切换它的开关。", "2D のままにするには「ゲーム」タブでスイッチを切り替えてください。"),
"Actualizando OdysseyVK": ("Updating OdysseyVK", "Mise à jour d'OdysseyVK", "OdysseyVK wird aktualisiert", "正在更新 OdysseyVK", "OdysseyVK を更新中"),
"Se baja de GitHub y se compila; luego se pide tu clave para instalar.": ("Downloading from GitHub and building; then your password is asked to install.", "Téléchargement depuis GitHub et compilation ; puis le mot de passe est demandé pour installer.", "Lädt von GitHub und kompiliert; danach wird zum Installieren das Passwort abgefragt.", "正在从 GitHub 下载并编译；随后需要输入密码进行安装。", "GitHub から取得してビルドします。その後インストールのためパスワードを求めます。"),
"OdysseyVK: la actualización falló": ("OdysseyVK: the update failed", "OdysseyVK : la mise à jour a échoué", "OdysseyVK: Aktualisierung fehlgeschlagen", "OdysseyVK：更新失败", "OdysseyVK：更新に失敗しました"),
"No se pudo bajar el repositorio (¿hay conexión y git?). Detalle en {}": ("Couldn't download the repository (connection and git?). Details in {}", "Impossible de télécharger le dépôt (connexion et git ?). Détails dans {}", "Repository konnte nicht geladen werden (Verbindung und git?). Details in {}", "无法下载仓库（网络连接和 git？）。详情见 {}", "リポジトリを取得できません（接続と git？）。詳細：{}"),
"No se pudo compilar o instalar. Detalle en {}": ("Couldn't build or install. Details in {}", "Impossible de compiler ou d'installer. Détails dans {}", "Kompilieren oder Installieren fehlgeschlagen. Details in {}", "无法编译或安装。详情见 {}", "ビルドまたはインストールに失敗しました。詳細：{}"),
"OdysseyVK {} instalado": ("OdysseyVK {} installed", "OdysseyVK {} installé", "OdysseyVK {} installiert", "OdysseyVK {} 已安装", "OdysseyVK {} をインストールしました"),
"El servicio se ha reiniciado con la versión nueva.": ("The service restarted with the new version.", "Le service a redémarré avec la nouvelle version.", "Der Dienst wurde mit der neuen Version neu gestartet.", "服务已用新版本重启。", "サービスは新しいバージョンで再起動しました。"),
"Instalado, pero el servicio no corre: systemctl --user restart odysseyvk-serve": ("Installed, but the service isn't running: systemctl --user restart odysseyvk-serve", "Installé, mais le service ne tourne pas : systemctl --user restart odysseyvk-serve", "Installiert, aber der Dienst läuft nicht: systemctl --user restart odysseyvk-serve", "已安装，但服务未运行：systemctl --user restart odysseyvk-serve", "インストール済みですがサービスが動いていません：systemctl --user restart odysseyvk-serve"),
"Desinstalando OdysseyVK": ("Uninstalling OdysseyVK", "Désinstallation d'OdysseyVK", "OdysseyVK wird deinstalliert", "正在卸载 OdysseyVK", "OdysseyVK をアンインストール中"),
"Se pide tu clave para quitar el programa del sistema.": ("Your password is needed to remove the program from the system.", "Le mot de passe est demandé pour retirer le programme du système.", "Zum Entfernen des Programms wird das Passwort abgefragt.", "需要输入密码以从系统中移除程序。", "システムからプログラムを削除するためパスワードを求めます。"),
"OdysseyVK desinstalado": ("OdysseyVK uninstalled", "OdysseyVK désinstallé", "OdysseyVK deinstalliert", "OdysseyVK 已卸载", "OdysseyVK をアンインストールしました"),
"OdysseyVK: quedó el programa del sistema": ("OdysseyVK: the system program is still there", "OdysseyVK : le programme système est resté", "OdysseyVK: das Systemprogramm ist noch da", "OdysseyVK：系统程序仍然保留", "OdysseyVK：システムのプログラムが残っています"),
"Tu configuración sigue en {}/odysseyvk.": ("Your settings remain in {}/odysseyvk.", "Vos réglages restent dans {}/odysseyvk.", "Deine Einstellungen bleiben in {}/odysseyvk.", "你的配置仍在 {}/odysseyvk。", "設定は {}/odysseyvk に残っています。"),
"No se dio la clave: queda {} y la regla udev.": ("No password given: {} and the udev rule remain.", "Mot de passe non saisi : {} et la règle udev restent.", "Kein Passwort: {} und die udev-Regel bleiben.", "未输入密码：{} 和 udev 规则仍保留。", "パスワードが入力されず、{} と udev ルールが残っています。"),
"ATAJOS DE TECLADO": ("KEYBOARD SHORTCUTS", "RACCOURCIS CLAVIER", "TASTENKÜRZEL", "键盘快捷键", "キーボードショートカット"),
"Los atajos valen aunque el teclado lo tenga el vídeo o el juego": ("Shortcuts work even when the video or game has the keyboard", "Les raccourcis marchent même si la vidéo ou le jeu a le clavier", "Die Kürzel gehen auch, wenn Video oder Spiel die Tastatur hat", "即使键盘焦点在视频或游戏上，快捷键也有效", "動画やゲームがキーボードを使っていてもショートカットが有効"),
"3D encendido o apagado": ("3D on or off", "3D activée ou désactivée", "3D ein oder aus", "开关 3D", "3D のオン／オフ"),
"Menos profundidad": ("Less depth", "Moins de profondeur", "Weniger Tiefe", "减少深度", "奥行きを減らす"),
"Más profundidad": ("More depth", "Plus de profondeur", "Mehr Tiefe", "增加深度", "奥行きを増やす"),
"Menos salida": ("Less pop-out", "Moins de jaillissement", "Weniger Pop-out", "减少出屏", "飛び出しを減らす"),
"Más salida": ("More pop-out", "Plus de jaillissement", "Mehr Pop-out", "增加出屏", "飛び出しを増やす"),
"Pastilla más chica": ("Smaller pill", "Pastille plus petite", "Kleinere Leiste", "缩小控制条", "ピルを小さく"),
"Pastilla más grande": ("Bigger pill", "Pastille plus grande", "Größere Leiste", "放大控制条", "ピルを大きく"),
"Ocultar o mostrar la pastilla": ("Hide or show the pill", "Masquer ou afficher la pastille", "Leiste aus- oder einblenden", "隐藏或显示控制条", "ピルの表示／非表示"),
"Pulsa las teclas…": ("Press the keys…", "Appuyez sur les touches…", "Tasten drücken…", "请按键…", "キーを押してください…"),
"sin cambios": ("no changes", "aucun changement", "keine Änderung", "未更改", "変更なし"),
"{}: el atajo necesita Ctrl, Alt o Meta": ("{}: the shortcut needs Ctrl, Alt or Meta", "{} : le raccourci doit avoir Ctrl, Alt ou Meta", "{}: das Kürzel braucht Strg, Alt oder Meta", "{}：快捷键需要包含 Ctrl、Alt 或 Meta", "{}：ショートカットには Ctrl、Alt、Meta のいずれかが必要です"),
"{} ya lo usa «{}»": ("{} is already used by “{}”", "{} est déjà utilisé par « {} »", "{} ist schon „{}“ zugewiesen", "{} 已被“{}”使用", "{} は「{}」で使われています"),
"«{}» ahora va con {}": ("“{}” is now {}", "« {} » est maintenant {}", "„{}“ ist jetzt {}", "“{}”现在是 {}", "「{}」は {} になりました"),
"«{}» vuelve a {}": ("“{}” is back to {}", "« {} » revient à {}", "„{}“ ist wieder {}", "“{}”恢复为 {}", "「{}」を {} に戻しました"),
"Pulsa la combinación nueva para «{}» · Esc cancela": ("Press the new combination for “{}” · Esc cancels", "Appuyez sur la nouvelle combinaison pour « {} » · Échap annule", "Neue Tastenkombination für „{}“ drücken · Esc bricht ab", "请为“{}”按下新的组合键 · Esc 取消", "「{}」の新しい組み合わせを押してください · Esc で取消"),
"salida": ("pop-out", "jaillissement", "Pop-out", "出屏", "飛び出し"),
"oculta": ("hides", "masque", "ausblenden", "隐藏", "隠す"),
"sale": ("exits", "quitte", "beendet", "退出", "終了"),
"Experimental": ("Experimental", "Expérimental", "Experimentell", "实验", "実験", ),
"Pruebas de la rama experimental: cada una se compara contra lo de fábrica.": ("Experimental-branch trials: each one is compared against the factory setting.", "Essais de la branche expérimentale : chacun se compare au réglage d'usine.", "Versuche des Experimental-Zweigs: jeder wird mit der Werkseinstellung verglichen.", "实验分支的试验：每项都与出厂设置对比。", "実験ブランチの試験：それぞれ初期設定と比較します。", ),
"Se aplican desde el siguiente vídeo. Lo que funcione pasará a la versión normal.": ("They apply from the next video. What works will go into the normal version.", "Ils s'appliquent dès la prochaine vidéo. Ce qui marche passera dans la version normale.", "Sie gelten ab dem nächsten Video. Was funktioniert, kommt in die normale Version.", "从下一个视频起生效。有效的将进入正式版本。", "次の動画から適用。うまくいったものは通常版に入ります。", ),
"La red ve el cuadro a su proporción": ("The network sees the frame at its proportion", "Le réseau voit l'image à sa proportion", "Das Netz sieht das Bild in seinem Seitenverhältnis", "网络按原比例看画面", "ネットワークは画面を元の比率で見る", ),
"Cuadrado": ("Square", "Carré", "Quadrat", "方形", "正方形", ),
"De fábrica la imagen 16:9 se aplasta a un cuadrado de 518; a su proporción la red la ve sin deformar, y más grande ve mejor lo delgado (más tiempo por mapa).": ("By default the 16:9 image is squashed into a 518 square; at its proportion the network sees it undistorted, and larger it sees thin things better (more time per map).", "D'usine l'image 16:9 est écrasée en un carré de 518 ; à sa proportion le réseau la voit sans déformation, et plus grande il voit mieux le fin (plus de temps par carte).", "Ab Werk wird das 16:9-Bild in ein 518er-Quadrat gestaucht; im Seitenverhältnis sieht das Netz es unverzerrt, und größer sieht es Dünnes besser (mehr Zeit pro Karte).", "出厂设置把 16:9 画面压成 518 的方形；按原比例网络看到不变形的画面，更大时细小物体更清楚（每张图更费时）。", "初期設定では 16:9 の画面を 518 の正方形に潰します。元の比率なら歪みなく、大きくすると細いものがよく見えます（1 マップの時間は増加）。", ),
"Tamaño del mapa de profundidad": ("Depth map size", "Taille de la carte de profondeur", "Größe der Tiefenkarte", "深度图大小", "深度マップのサイズ", ),
"Puntos a lo ancho del mapa que llega a la trama: más puntos, bordes más finos y más tiempo de limpieza.": ("Points across the map the weave receives: more points, finer edges and more clean-up time.", "Points en largeur de la carte reçue par la trame : plus de points, bords plus fins et plus de temps de nettoyage.", "Punkte über die Breite der Karte, die das Weben erhält: mehr Punkte, feinere Kanten und mehr Zeit fürs Säubern.", "交织所用深度图的横向点数：点越多，边缘越细，清理越费时。", "ウィーブに渡すマップの横方向の点数：多いほど縁が細かく、整理に時間がかかります。", ),
"SUAVIZADO EN EL TIEMPO": ("SMOOTHING OVER TIME", "LISSAGE DANS LE TEMPS", "GLÄTTUNG ÜBER DIE ZEIT", "时间平滑", "時間方向の平滑化", ),
"Tope de arrastre": ("Lag cap", "Plafond de traînée", "Nachzieh-Grenze", "拖尾上限", "遅れの上限", ),
"Ningún punto se queda más de 1/16 de la escala por detrás del mapa nuevo: lo que se mueve no deja estela": ("No point stays more than 1/16 of the range behind the new map: what moves leaves no trail", "Aucun point ne reste à plus de 1/16 de l'échelle derrière la nouvelle carte : ce qui bouge ne laisse pas de traînée", "Kein Punkt bleibt mehr als 1/16 des Bereichs hinter der neuen Karte zurück: was sich bewegt, zieht keine Spur", "任何点落后新图不超过量程的 1/16：移动的物体不留拖尾", "どの点も新しいマップから範囲の 1/16 以上遅れない：動くものが尾を引きません", ),
"SEGUIMIENTO DE LA CABEZA": ("HEAD TRACKING", "SUIVI DE LA TÊTE", "KOPFVERFOLGUNG", "头部追踪", "頭部トラッキング", ),
"Anticipar la cabeza": ("Anticipate the head", "Anticiper la tête", "Kopf vorausberechnen", "预测头部位置", "頭の動きを先読み", ),
"La trama se calcula para donde estarán los ojos dentro de estos milisegundos (0 = apagado; Any2Odyssey usa 25)": ("The weave is computed for where the eyes will be in this many milliseconds (0 = off; Any2Odyssey uses 25)", "La trame est calculée pour là où seront les yeux dans ce nombre de millisecondes (0 = désactivé ; Any2Odyssey utilise 25)", "Das Weben wird für die Augenposition in so vielen Millisekunden berechnet (0 = aus; Any2Odyssey nutzt 25)", "按这些毫秒后眼睛所在位置计算交织（0 = 关闭；Any2Odyssey 用 25）", "この時間後の目の位置に合わせてウィーブを計算（0 = オフ、Any2Odyssey は 25）", ),
"anticipación {} ms · desde el siguiente vídeo": ("anticipation {} ms · from the next video", "anticipation {} ms · dès la prochaine vidéo", "Vorausberechnung {} ms · ab dem nächsten Video", "预测 {} 毫秒 · 从下一个视频起", "先読み {} ms · 次の動画から", ),
"VISOR": ("VIEWER", "VISIONNEUSE", "VIEWER", "查看器", "ビューア", ),
"Prioridad del visor en la tarjeta": ("Viewer priority on the GPU", "Priorité de la visionneuse sur la carte", "Priorität des Viewers auf der Grafikkarte", "查看器在显卡上的优先级", "GPU でのビューアの優先度", ),
"Normal": ("Normal", "Normale", "Normal", "普通", "通常", ),
"Alta": ("High", "Haute", "Hoch", "高", "高", ),
"Tiempo real": ("Real time", "Temps réel", "Echtzeit", "实时", "リアルタイム", ),
"Con un juego llenando la tarjeta, el cuadro 3D pasa por delante del juego y no espera su turno; el juego pierde un poco.": ("With a game filling the GPU, the 3D frame goes ahead of the game instead of waiting its turn; the game loses a little.", "Avec un jeu qui remplit la carte, l'image 3D passe devant le jeu au lieu d'attendre son tour ; le jeu perd un peu.", "Wenn ein Spiel die Grafikkarte auslastet, kommt das 3D-Bild vor dem Spiel dran statt zu warten; das Spiel verliert etwas.", "游戏占满显卡时，3D 画面优先于游戏处理，无需排队；游戏会稍有损失。", "ゲームが GPU を占有していても、3D の画面が順番を待たずに先に処理されます。ゲームは少し遅くなります。", ),
"MODO RENDIMIENTO": ("PERFORMANCE MODE", "MODE PERFORMANCE", "LEISTUNGSMODUS", "性能模式", "パフォーマンスモード", ),
"Profundidad del vídeo normal": ("Depth for normal video", "Profondeur de la vidéo normale", "Tiefe für normale Videos", "普通视频的深度", "通常動画の奥行き", ),
"Con la red": ("With the network", "Avec le réseau", "Mit dem Netz", "使用网络", "ネットワークで", ),
"Frecuencia variable (VRR) durante el 3D": ("Variable refresh (VRR) during 3D", "Fréquence variable (VRR) pendant la 3D", "Variable Bildrate (VRR) während 3D", "3D 时启用可变刷新率（VRR）", "3D 中の可変リフレッシュレート（VRR）", ),
"De fábrica se apaga: con ella, los juegos por debajo del refresco daban tirones": ("Off by default: with it, games below the refresh rate stuttered", "Désactivée d'usine : avec elle, les jeux sous la fréquence saccadaient", "Ab Werk aus: damit ruckelten Spiele unter der Bildrate", "默认关闭：开启时，低于刷新率的游戏会卡顿", "初期設定ではオフ：オンだとリフレッシュレート未満のゲームがカクついていました", ),
"Paralaje de la cabeza": ("Head parallax", "Parallaxe de la tête", "Kopfparallaxe", "头部视差", "頭の視差", ),
"Sin red": ("Without the network", "Sans réseau", "Ohne Netz", "无网络", "ネットなし", ),
"Mezcla: suelo ↔ imagen": ("Mix: floor ↔ image", "Mélange : sol ↔ image", "Mischung: Boden ↔ Bild", "混合：地面 ↔ 画面", "配合：床 ↔ 画像", ),
"Suave": ("Gentle", "Doux", "Sanft", "柔和", "ソフト", ),
"Medio": ("Medium", "Moyen", "Mittel", "适中", "標準", ),
"Intenso": ("Intense", "Intense", "Intensiv", "强烈", "強め", ),
"A mano": ("Custom", "Personnalisé", "Eigene", "自定义", "カスタム", ),
"Ctrl+Shift+  1 el 3D · F1 oculta · Esc sale": ("Ctrl+Shift+  1 3D · F1 hides · Esc exits", "Ctrl+Maj+  1 la 3D · F1 masque · Échap quitte", "Strg+Umschalt+  1 3D · F1 ausblenden · Esc beendet", "Ctrl+Shift+  1 开关3D · F1 隐藏 · Esc 退出", "Ctrl+Shift+  1 3D · F1 隠す · Esc 終了", ),
"Volver a la red: Ajustes → Experimental": ("Back to the network: Settings → Experimental", "Revenir au réseau : Réglages → Expérimental", "Zurück zum Netz: Einstellungen → Experimentell", "改回网络：设置 → 实验", "ネットワークに戻す：設定 → 実験", ),
"Alcance del relieve": ("Depth reach", "Portée du relief", "Tiefenreichweite", "立体范围", "立体の範囲", ),
"La red Depth Anything: la forma de verdad de cada objeto. Ocupa la tarjeta: ~1,1 GB y 14-18 ms por mapa.": ("The Depth Anything network: the real shape of each object. Uses the GPU: ~1.1 GB and 14-18 ms per map.", "Le réseau Depth Anything : la vraie forme de chaque objet. Occupe la carte : ~1,1 Go et 14-18 ms par carte.", "Das Netz Depth Anything: die echte Form jedes Objekts. Belegt die Grafikkarte: ~1,1 GB und 14-18 ms pro Karte.", "Depth Anything 网络：每个物体的真实形状。占用显卡：约 1.1 GB，每张图 14-18 毫秒。", "Depth Anything ネットワーク：各物体の本当の形。GPU を使用：約 1.1 GB、1 マップ 14〜18 ms。", ),
"La profundidad sale de la propia imagen en el procesador (~9 ms por mapa), sin tocar la tarjeta: más basta, pero más estable. La pastilla lleva un rayo con su menú (los preajustes y los ajustes finos), y la frecuencia variable queda como en el escritorio.": ("Depth comes from the image itself on the CPU (~9 ms per map), without touching the GPU: rougher, but steadier. The pill gets a lightning bolt with its menu (the presets and the fine settings), and variable refresh stays as on the desktop.", "La profondeur vient de l'image elle-même, sur le processeur (~9 ms par carte), sans toucher la carte : plus grossière mais plus stable. La pastille porte un éclair avec son menu (les préréglages et les réglages fins), et la fréquence variable reste comme sur le bureau.", "Die Tiefe kommt aus dem Bild selbst, auf der CPU (~9 ms pro Karte), ohne Grafikkarte: gröber, aber ruhiger. Die Leiste bekommt einen Blitz mit seinem Menü (die Voreinstellungen und die Feineinstellungen), und die variable Bildrate bleibt wie auf dem Desktop.", "深度由处理器从画面本身得出（每张图约 9 毫秒），不占用显卡：更粗糙但更稳定。控制条会显示闪电及其菜单（预设和细调），可变刷新率保持与桌面相同。", "奥行きは画像そのものから CPU で計算（1 マップ約 9 ms）、GPU は使いません。粗いですが安定。ピルに稲妻とそのメニュー（プリセットと細かい調整）が付き、可変リフレッシュレートはデスクトップのままです。", ),
"Mezcla 25 % · relieve x0,75 · topes x2 · paralaje x1,5. Hasta ~32 px entre los ojos y casi nada aplastado: para sesiones largas.": ("Mix 25 % · depth x0.75 · limits x2 · parallax x1.5. Up to ~32 px between the eyes and almost nothing flattened: for long sessions.", "Mélange 25 % · relief x0,75 · limites x2 · parallaxe x1,5. Jusqu'à ~32 px entre les yeux, presque rien d'aplati : pour les longues sessions.", "Mischung 25 % · Tiefe x0,75 · Grenzen x2 · Parallaxe x1,5. Bis ~32 px zwischen den Augen, fast nichts abgeflacht: für lange Sitzungen.", "混合 25 % · 立体 x0.75 · 上限 x2 · 视差 x1.5。双眼间距最多约 32 像素，几乎不压平：适合长时间使用。", "配合 25 % · 立体 x0.75 · 上限 x2 · 視差 x1.5。左右のずれ最大約 32 px、ほぼ潰れなし：長時間向け。", ),
"Mezcla 35 % · relieve x1 · topes x2,75 · paralaje x2. Hasta ~43 px entre los ojos y casi nada aplastado: el de fábrica.": ("Mix 35 % · depth x1 · limits x2.75 · parallax x2. Up to ~43 px between the eyes and almost nothing flattened: the default.", "Mélange 35 % · relief x1 · limites x2,75 · parallaxe x2. Jusqu'à ~43 px entre les yeux, presque rien d'aplati : le réglage d'usine.", "Mischung 35 % · Tiefe x1 · Grenzen x2,75 · Parallaxe x2. Bis ~43 px zwischen den Augen, fast nichts abgeflacht: die Werkseinstellung.", "混合 35 % · 立体 x1 · 上限 x2.75 · 视差 x2。双眼间距最多约 43 像素，几乎不压平：出厂设置。", "配合 35 % · 立体 x1 · 上限 x2.75 · 視差 x2。左右のずれ最大約 43 px、ほぼ潰れなし：初期設定。", ),
"Mezcla 50 % · relieve x1,5 · topes x2,75 · paralaje x3. Hasta ~52 px entre los ojos; un 20 % del cuadro toca el tope. Más 3D, cansa antes.": ("Mix 50 % · depth x1.5 · limits x2.75 · parallax x3. Up to ~52 px between the eyes; 20 % of the frame hits the limit. More 3D, tires sooner.", "Mélange 50 % · relief x1,5 · limites x2,75 · parallaxe x3. Jusqu'à ~52 px entre les yeux ; 20 % de l'image touche la limite. Plus de 3D, fatigue plus vite.", "Mischung 50 % · Tiefe x1,5 · Grenzen x2,75 · Parallaxe x3. Bis ~52 px zwischen den Augen; 20 % des Bildes erreichen die Grenze. Mehr 3D, ermüdet schneller.", "混合 50 % · 立体 x1.5 · 上限 x2.75 · 视差 x3。双眼间距最多约 52 像素；20 % 的画面触及上限。3D 更强，更易疲劳。", "配合 50 % · 立体 x1.5 · 上限 x2.75 · 視差 x3。左右のずれ最大約 52 px、画面の 20 % が上限に当たります。3D は強く、疲れやすい。", ),
"Hacia 0 %: un plano inclinado, estable, igual en cada cuadro. Hacia 100 %: lo cercano se separa más del fondo, pero también sale hacia delante lo nítido aunque esté lejos (los subtítulos).": ("Towards 0 %: a tilted plane, steady, the same every frame. Towards 100 %: near things separate more from the background, but sharp things also pop out even when far (subtitles).", "Vers 0 % : un plan incliné, stable, identique à chaque image. Vers 100 % : le proche se détache plus du fond, mais le net ressort aussi même s'il est loin (les sous-titres).", "Richtung 0 %: eine geneigte Ebene, ruhig, in jedem Bild gleich. Richtung 100 %: Nahes löst sich stärker vom Hintergrund, aber auch Scharfes tritt hervor, selbst wenn es fern ist (Untertitel).", "接近 0 %：一个倾斜的平面，稳定，每帧相同。接近 100 %：近处更脱离背景，但清晰的东西即使在远处也会凸出（字幕）。", "0 % 寄り：傾いた平面で安定、どのフレームも同じ。100 % 寄り：近くのものが背景から離れますが、遠くても鮮明なもの（字幕）も飛び出します。", ),
"Más fuerza = más 3D. Pero si la fuerza pasa del alcance, el relieve se aplasta contra el tope y se ve como recortes planos: súbelo junto con el alcance.": ("More strength = more 3D. But if strength exceeds the reach, depth flattens against the limit and looks like flat cut-outs: raise it together with the reach.", "Plus de force = plus de 3D. Mais si la force dépasse la portée, le relief s'écrase contre la limite et ressemble à des découpes plates : augmentez-la avec la portée.", "Mehr Stärke = mehr 3D. Übersteigt die Stärke die Reichweite, wird die Tiefe an der Grenze plattgedrückt und wirkt wie flache Ausschnitte: zusammen mit der Reichweite erhöhen.", "强度越大 3D 越强。但强度超过范围时，立体会被压在上限上，看起来像扁平的剪纸：请与范围一起调高。", "強いほど 3D が増します。ただし範囲を超えると上限で潰れ、平たい切り絵のように見えます。範囲と一緒に上げてください。", ),
"Los topes por ojo (+12,67 / -9,98 px de fábrica), multiplicados. Más alcance deja más 3D sin aplastar; demasiado cansa la vista (unos 98 px entre los ojos ya molestaban).": ("The per-eye limits (+12.67 / -9.98 px by default), multiplied. More reach allows more 3D without flattening; too much tires the eyes (about 98 px between the eyes was already uncomfortable).", "Les limites par œil (+12,67 / -9,98 px d'usine), multipliées. Plus de portée laisse plus de 3D sans écraser ; trop fatigue les yeux (environ 98 px entre les yeux gênaient déjà).", "Die Grenzen pro Auge (ab Werk +12,67 / -9,98 px), multipliziert. Mehr Reichweite erlaubt mehr 3D ohne Abflachen; zu viel ermüdet die Augen (etwa 98 px zwischen den Augen störten bereits).", "每只眼的上限（出厂 +12.67 / -9.98 像素）乘以此倍数。范围越大，3D 越不易被压平；过大会使眼睛疲劳（双眼间距约 98 像素时已感到不适）。", "片目ごとの上限（初期 +12.67 / -9.98 px）を倍増。範囲が広いほど潰れずに 3D が出ますが、広すぎると目が疲れます（左右で約 98 px ずれると不快でした）。", ),
"Con `paralaje = 0,5` en serve.conf, x2 es la geometría real. Más exagera el efecto de ventana al mover la cabeza; menos lo calma.": ("With `paralaje = 0,5` in serve.conf, x2 is the real geometry. More exaggerates the window effect when moving your head; less calms it.", "Avec `paralaje = 0,5` dans serve.conf, x2 est la géométrie réelle. Plus exagère l'effet de fenêtre en bougeant la tête ; moins le calme.", "Mit `paralaje = 0,5` in serve.conf ist x2 die echte Geometrie. Mehr übertreibt den Fenstereffekt beim Kopfbewegen; weniger beruhigt ihn.", "serve.conf 中 `paralaje = 0,5` 时，x2 即真实几何。更高会夸大移动头部时的窗口效果；更低则减弱。", "serve.conf が `paralaje = 0,5` なら x2 が実際の幾何。上げると頭を動かしたときの窓の効果が強まり、下げると穏やかになります。", ),
"Con ella la pantalla sigue el ritmo del juego. En Cyberpunk, por debajo de 100 cuadros daba tirones (25 % de los cuadros tarde); sin ella, 165 Hz fijos.": ("With it, the screen follows the game's pace. In Cyberpunk, below 100 fps it stuttered (25 % of frames late); without it, a fixed 165 Hz.", "Avec elle, l'écran suit le rythme du jeu. Dans Cyberpunk, sous 100 images/s ça saccadait (25 % des images en retard) ; sans elle, 165 Hz fixes.", "Damit folgt der Bildschirm dem Takt des Spiels. In Cyberpunk ruckelte es unter 100 Bildern/s (25 % der Bilder zu spät); ohne sie feste 165 Hz.", "开启后屏幕跟随游戏节奏。在《赛博朋克》中低于 100 帧时会卡顿（25 % 的帧延迟）；关闭则固定 165 Hz。", "オンだと画面がゲームのペースに合わせます。Cyberpunk では 100 fps 未満でカクつきました（25 % のフレームが遅延）。オフなら 165 Hz 固定。", ),
"el idioma cambia al momento": ("the language changes right away", "la langue change tout de suite", "die Sprache wechselt sofort", "语言立即切换", "言語はすぐに切り替わります"),
"Versión ligera de la red": ("Lighter version of the network", "Version légère du réseau", "Leichtere Version des Netzes", "网络的轻量版本", "ネットワークの軽量版", ),
"La misma red a media precisión: el mismo mapa con un 35 % menos de tiempo y 260 MB menos en la tarjeta.": ("The same network at half precision: the same map with 35 % less time and 260 MB less on the GPU.", "Le même réseau en demi-précision : la même carte avec 35 % de temps en moins et 260 Mo de moins sur la carte graphique.", "Dasselbe Netz mit halber Genauigkeit: dieselbe Karte mit 35 % weniger Zeit und 260 MB weniger auf der Grafikkarte.", "同一网络的半精度版本：深度图相同，耗时少 35 %，显存少 260 MB。", "同じネットワークの半精度版：同じマップで、時間は 35 % 短く、GPU メモリは 260 MB 少なくなります。", ),
"Hasta dónde salen y se hunden las cosas con la red · x1 = los topes de Samsung · en Auto se calcula solo": ("How far things pop out and sink in with the network · x1 = Samsung's limits · in Auto it is worked out by itself", "Jusqu'où les objets sortent et s'enfoncent avec le réseau · x1 = les limites de Samsung · en Auto, calculée toute seule", "Wie weit Dinge mit dem Netz heraustreten und versinken · x1 = Samsungs Grenzen · in Auto automatisch berechnet", "使用网络时物体出屏和入屏的幅度 · x1 = 三星的限制 · 自动模式下自动计算", "ネットワーク使用時に物が飛び出し・沈み込む範囲 · x1 = Samsung の上限 · オートでは自動で計算", ),
"Subir la fuerza o la Profundidad no da más 3D si la imagen ya choca con el tope: se aplasta. Medido con Profundidad 100 y relieve x2: a x1 el 85 % del cuadro está pegado al tope (unos 11 px de relieve entre los ojos); a x2,75, el 58 % (47 px); a x4, el 32 % (71 px). Más de ~98 px cansa la vista.": ("Raising the strength or the Depth gives no more 3D once the image hits the limit: it flattens. Measured with Depth 100 and relief x2: at x1, 85 % of the frame sits on the limit (about 11 px of relief between the eyes); at x2.75, 58 % (47 px); at x4, 32 % (71 px). More than ~98 px tires the eyes.", "Augmenter la force ou la Profondeur ne donne pas plus de 3D si l'image touche déjà la limite : elle s'aplatit. Mesuré avec Profondeur 100 et relief x2 : à x1, 85 % de l'image colle à la limite (environ 11 px de relief entre les yeux) ; à x2,75, 58 % (47 px) ; à x4, 32 % (71 px). Plus de ~98 px fatigue les yeux.", "Mehr Stärke oder Tiefe bringt kein zusätzliches 3D, wenn das Bild schon an die Grenze stößt: es wird flach. Gemessen mit Tiefe 100 und Relief x2: bei x1 liegen 85 % des Bildes an der Grenze (etwa 11 px Relief zwischen den Augen); bei x2,75 58 % (47 px); bei x4 32 % (71 px). Mehr als ~98 px ermüdet die Augen.", "画面已经碰到上限时，再加强度或深度也不会更立体，只会被压平。在深度 100、立体 x2 下实测：x1 时 85 % 的画面贴在上限（双眼间约 11 px 的立体）；x2.75 时 58 %（47 px）；x4 时 32 %（71 px）。超过约 98 px 眼睛会累。", "画像がすでに上限に当たっていると、強さや奥行きを上げても立体感は増えず、平らに潰れます。奥行き 100、立体 x2 で実測：x1 では画面の 85 % が上限に張り付き（両目の間で約 11 px）、x2.75 では 58 %（47 px）、x4 では 32 %（71 px）。約 98 px を超えると目が疲れます。", ),
"alcance x{}: el vídeo abierto lo toma al momento": ("reach x{}: the open video takes it right away", "portée x{} : la vidéo ouverte l'applique tout de suite", "Reichweite x{}: das offene Video übernimmt sie sofort", "范围 x{}：已打开的视频立即生效", "範囲 x{}：開いている動画にすぐ反映されます", ),
"Fijado encima: un clic en el juego no cierra el panel. Pica para soltarlo.": ("Pinned on top: a click on the game doesn't close the panel. Click to unpin.", "Épinglé au-dessus : un clic sur le jeu ne ferme pas le panneau. Cliquez pour le détacher.", "Oben angeheftet: Ein Klick ins Spiel schließt das Fenster nicht. Klicken zum Lösen.", "已置顶：点击游戏不会关闭面板。点击取消置顶。", "最前面に固定：ゲームをクリックしてもパネルは閉じません。クリックで解除。", ),
"Fijar encima: que un clic en el juego no cierre el panel.": ("Pin on top: a click on the game won't close the panel.", "Épingler au-dessus : un clic sur le jeu ne fermera pas le panneau.", "Oben anheften: Ein Klick ins Spiel schließt das Fenster nicht.", "置顶：点击游戏时不关闭面板。", "最前面に固定：ゲームをクリックしてもパネルを閉じない。", ),
"Auto": ("Auto", "Auto", "Auto", "自动", "オート", ),
"Manual": ("Manual", "Manuel", "Manuell", "手动", "手動", ),
"RELIEVE": ("RELIEF", "RELIEF", "RELIEF", "立体", "立体感", ),
"Tipo de vídeo": ("Video type", "Type de vidéo", "Videotyp", "视频类型", "動画の種類", ),
"Plano {} · Relieve {} px · Alcance x{}": ("Plane {} · Relief {} px · Reach x{}", "Plan {} · Relief {} px · Portée x{}", "Ebene {} · Relief {} px · Reichweite x{}", "平面 {} · 立体 {} px · 范围 x{}", "面 {} · 立体感 {} px · 範囲 x{}", ),
"La saliencia elige el plano de la pantalla en cada escena; el alcance se ajusta a la distancia de tus ojos (±1° de paralaje, la zona cómoda) y la salida se calcula para el mayor relieve sin aplastar la imagen contra él.": ("Saliency picks the screen plane in each scene; the reach follows the distance of your eyes (±1° of parallax, the comfortable zone) and the pop-out is worked out for the most relief without flattening the image against it.", "La saillance choisit le plan de l'écran dans chaque scène ; la portée suit la distance de vos yeux (±1° de parallaxe, la zone confortable) et le jaillissement est calculé pour le plus de relief sans aplatir l'image contre elle.", "Die Salienz wählt in jeder Szene die Bildschirmebene; die Reichweite folgt dem Abstand deiner Augen (±1° Parallaxe, die bequeme Zone), und das Pop-out wird für das meiste Relief berechnet, ohne das Bild daran platt zu drücken.", "显著性在每个场景中选择屏幕平面；范围随你眼睛的距离调整（±1° 视差，舒适区），出屏量自动计算，在不压平画面的前提下得到最大立体感。", "顕著性が各シーンで画面の面を選び、範囲は目の距離に合わせ（視差 ±1°、快適な範囲）、飛び出しは画像を潰さずに最大の立体感になるよう計算されます。", ),
"Auto: en cada escena la saliencia busca lo que más llama la atención (una cara, un objeto) y lo deja en el plano de la pantalla, nítido. El alcance se ajusta a la distancia de tus ojos para no pasar de 1° de paralaje (la zona cómoda de la vista) y la salida se calcula para el mayor relieve que cabe sin aplastar la imagen. Sigue al contenido y salta en los cortes de escena.": ("Auto: in each scene saliency finds what draws the eye most (a face, an object) and puts it on the screen plane, sharp. The reach follows the distance of your eyes so parallax stays within 1° (the eyes' comfortable zone), and the pop-out is worked out for the most relief that fits without flattening the image. It follows the content and jumps at scene cuts.", "Auto : dans chaque scène, la saillance trouve ce qui attire le plus le regard (un visage, un objet) et le place, net, sur le plan de l'écran. La portée suit la distance de vos yeux pour ne pas dépasser 1° de parallaxe (la zone confortable de la vue) et le jaillissement est calculé pour le plus de relief possible sans aplatir l'image. Il suit le contenu et saute aux changements de plan.", "Auto: In jeder Szene findet die Salienz, was am meisten auffällt (ein Gesicht, ein Objekt), und legt es scharf auf die Bildschirmebene. Die Reichweite folgt dem Abstand deiner Augen, damit die Parallaxe unter 1° bleibt (die bequeme Zone), und das Pop-out wird für das meiste Relief berechnet, das ohne Plattdrücken passt. Es folgt dem Inhalt und springt bei Schnitten.", "自动：在每个场景中，显著性找出最吸引目光的东西（一张脸、一个物体），把它清晰地放在屏幕平面上。范围随你眼睛的距离调整，使视差不超过 1°（眼睛的舒适区），出屏量计算为不压平画面时的最大立体感。它跟随内容，并在镜头切换时跳变。", "オート：各シーンで顕著性が最も目を引くもの（顔や物）を見つけ、画面の面にくっきり置きます。範囲は目の距離に合わせて視差が 1°（目の快適な範囲）を超えないようにし、飛び出しは画像を潰さない範囲で最大の立体感になるよう計算します。内容に追従し、シーンの切り替わりで切り替わります。", ),
"Manual: tú eliges el plano que queda en la pantalla (Profundidad) y cuánto sale y se hunde la imagen (Salida). Al pasar de Auto a Manual se quedan los valores que se estaban calculando.": ("Manual: you pick the plane that sits on the screen (Depth) and how far the image pops out and sinks in (Pop-out). Switching from Auto to Manual keeps the values that were being worked out.", "Manuel : vous choisissez le plan qui reste sur l'écran (Profondeur) et à quel point l'image sort et s'enfonce (Jaillissement). En passant d'Auto à Manuel, les valeurs calculées sont conservées.", "Manuell: Du wählst die Ebene auf dem Bildschirm (Tiefe) und wie weit das Bild heraustritt und versinkt (Pop-out). Beim Wechsel von Auto zu Manuell bleiben die berechneten Werte.", "手动：由你选择停在屏幕上的平面（深度）以及画面出屏和入屏的程度（出屏）。从自动切到手动时，保留正在计算的数值。", "手動：画面に置く面（奥行き）と、画像の飛び出し・沈み込みの量（飛び出し）を自分で選びます。オートから手動に切り替えると、計算中の値がそのまま残ります。", ),
"El tipo de vídeo se detecta solo": ("The video type is detected automatically", "Le type de vidéo est détecté tout seul", "Der Videotyp wird automatisch erkannt", "视频类型自动识别", "動画の種類は自動で判別", ),
"Al ponerse a pantalla completa, el visor mira los primeros segundos (normal, lado a lado o arriba y abajo) y luego deja de mirar; en la pastilla se puede cambiar a mano.": ("When it goes fullscreen, the viewer looks at the first seconds (normal, side by side or top and bottom) and then stops looking; it can be changed by hand on the pill.", "En passant en plein écran, le visualiseur regarde les premières secondes (normale, côte à côte ou dessus-dessous) puis arrête ; on peut le changer à la main sur la pastille.", "Beim Vollbild schaut der Viewer die ersten Sekunden an (normal, nebeneinander oder übereinander) und hört dann auf; auf der Leiste lässt es sich von Hand ändern.", "全屏时，查看器会观察开头几秒（普通、左右或上下），之后不再检查；可以在控制条上手动更改。", "全画面になると、ビューアーが最初の数秒を見て（通常・左右・上下）判別し、その後は見ません。ピルで手動でも変えられます。", ),
}

from traducciones_ru import RU
assert len(RU) == len(T), (len(RU), len(T))
T = {k: v + (r,) for (k, v), r in zip(T.items(), RU)}

def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')

def revisar():
    """Cada texto que el codigo pasa por idioma::tr / idioma::trf tiene que estar en la tabla."""
    import os, re, glob
    raiz = os.path.join(os.path.dirname(__file__), "..", "src")
    lit = re.compile(r'"((?:[^"\\]|\\.)*)"')
    faltan = []
    vistos = 0
    for f in sorted(glob.glob(os.path.join(raiz, "*.cpp"))):
        txt = open(f, encoding="utf-8").read()
        for m in re.finditer(r'idioma::trf?\(', txt):
            i, prof = m.end(), 1
            while i < len(txt) and prof:
                c = txt[i]
                if c == '"':
                    j = i + 1
                    while txt[j] != '"':
                        j += 2 if txt[j] == "\\" else 1
                    i = j
                elif c == '(':
                    prof += 1
                elif c == ')':
                    prof -= 1
                i += 1
            arg = txt[m.end():i - 1]
            if m.group(0).startswith("idioma::trf"):
                arg = re.sub(r'\{[^{}]*\}\s*$', '', arg)   # la lista de datos no se traduce
            for t in lit.findall(arg):
                t = t.encode().decode("unicode_escape").encode("latin-1").decode("utf-8")
                vistos += 1
                if t not in T:
                    faltan.append((os.path.basename(f), txt.count("\n", 0, m.start()) + 1, t))
    # los que van en tablas del codigo y se traducen con tr(tabla[i]): a mano
    for t in ("los vídeos divididos se suavizan siempre", "con su propio relieve mientras quepa; suavizado solo si algo sale doble",
              "los vídeos divididos se ven siempre con su propio relieve", "vídeo normal",
              "vídeo dividido: el relieve del propio vídeo", "vídeo dividido suavizado: el relieve lo pone el conversor"):
        vistos += 1
        if t not in T:
            faltan.append(("tabla", 0, t))
    for f, n, t in faltan:
        print("FALTA %s:%d  %s" % (f, n, t))
    print(vistos, "textos en el codigo;", len(faltan), "sin traducir")
    return not faltan

if __name__ == "__main__":
    import os, sys
    if len(sys.argv) > 1 and sys.argv[1] == "revisar":
        sys.exit(0 if revisar() else 1)
    destino = os.path.join(os.path.dirname(__file__), "..", "src", "idioma_tabla.inc")
    with open(destino, "w", encoding="utf-8") as f:
        f.write("/* generado por tools/traducciones.py: no editar a mano */\n")
        for k, v in T.items():
            assert len(v) == 6, k
            f.write('{"%s", {"%s", "%s", "%s", "%s", "%s", "%s"}},\n' % ((esc(k),) + tuple(esc(x) for x in v)))
    print(len(T), "textos")
