import paho.mqtt.client as mqtt
import ssl
import time
import json
from datetime import datetime
from telegram import Update
from telegram.ext import Application, CommandHandler, MessageHandler, filters, ContextTypes, ConversationHandler

# HiveMQ Cloud connection details
HIVEMQ_HOST = "4b7ef9fcd7844435b6671b0aa5b80550.s1.eu.hivemq.cloud"
HIVEMQ_PORT = 8883
HIVEMQ_USERNAME = "Server"  # Thay bằng username của bạn
HIVEMQ_PASSWORD = "Hp123456"  # Thay bằng password của bạn

# MQTT Topics
TOPIC_PUBLISH = "ESP32_sub"
TOPIC_SUBSCRIBE = "ESP32_pub"

# Telegram Bot Token
TELEGRAM_TOKEN = "8587206474:AAHU7_vryZ0ChaL5I7WENzM-UmRqBV_Q0es"

# States cho conversation
WAITING_DEVICE_ID, WAITING_SPEED = range(2)

# Biến lưu trữ dữ liệu
latest_data = {}  # {device_id: {speed, lat, long, timestamp}}
user_devices = {}  # {user_id: device_id}
device_users = {}  # {device_id: [user_id1, user_id2, ...]} - Danh sách user theo dõi device
mqtt_client = None
telegram_app = None
bot_loop = None  # Event loop của bot

# Callback khi kết nối thành công
def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print("Kết nối thành công đến HiveMQ Cloud!")
        # Subscribe to topic sau khi kết nối thành công
        client.subscribe(TOPIC_SUBSCRIBE)
        print(f"Đã subscribe đến topic: {TOPIC_SUBSCRIBE}")
    else:
        print(f"Kết nối thất bại với mã lỗi: {reason_code}")

# Callback khi nhận được message
def on_message(client, userdata, msg):
    global latest_data, telegram_app
    
    try:
        # Thử parse dạng CSV
        raw_data = msg.payload.decode().strip()
        parts = raw_data.split(',')
        
        # Kiểm tra nếu là cảnh báo FALL: id,FALL
        if len(parts) == 2 and parts[1].upper() == "FALL":
            device_id = parts[0]
            print(f"⚠️ CẢNH BÁO NGÃ: {device_id}")
            
            # Gửi cảnh báo đến tất cả user đang theo dõi device này
            if device_id in device_users and telegram_app and bot_loop:
                import asyncio
                for user_id in device_users[device_id]:
                    print(f"📤 Đang gửi cảnh báo đến user {user_id}...")
                    # Tạo coroutine và schedule nó vào event loop của bot
                    asyncio.run_coroutine_threadsafe(
                        send_fall_alert(user_id, device_id),
                        bot_loop
                    )
            else:
                if device_id not in device_users:
                    print(f"ℹ️ Không có user nào theo dõi thiết bị {device_id}")
                if not telegram_app:
                    print(f"ℹ️ Telegram app chưa sẵn sàng")
                if not bot_loop:
                    print(f"ℹ️ Bot event loop chưa sẵn sàng")
            
            return
        
        # Parse dạng CSV: id,speed,lat,long
        if len(parts) == 4:
            device_id = parts[0]
            speed = float(parts[1])
            lat = float(parts[2])
            long = float(parts[3])
            timestamp = time.time()
            
            # Tạo JSON object từ dữ liệu CSV
            data_json = {
                "id": device_id,
                "speed": speed,
                "lat": lat,
                "long": long,
                "timestamp": timestamp,
                "datetime": datetime.fromtimestamp(timestamp).strftime("%d/%m/%Y %H:%M:%S")
            }
            
            # Lưu vào latest_data
            latest_data[device_id] = data_json
            
            # Lưu vào file tự động
            with open('mqtt_data.json', 'a', encoding='utf-8') as f:
                f.write(json.dumps(data_json) + '\n')
            
            print(f"✅ Đã lưu dữ liệu từ {device_id}")
                
    except ValueError as e:
        print(f"❌ Lỗi parse: {e}")
    except Exception as e:
        print(f"❌ Lỗi xử lý: {e}")

# Callback khi publish thành công
def on_publish(client, userdata, mid, reason_code, properties):
    pass  # Không in log nữa

# Callback khi disconnect
def on_disconnect(client, userdata, rc):
    if rc != 0:
        print(f"Mất kết nối bất ngờ. Mã lỗi: {rc}")
    else:
        print("Đã ngắt kết nối")

# ========== TELEGRAM BOT HANDLERS ==========

# Hàm gửi cảnh báo ngã
async def send_fall_alert(user_id: int, device_id: str):
    """Gửi cảnh báo ngã đến người dùng qua Telegram"""
    try:
        if telegram_app:
            # Lấy tọa độ gần nhất nếu có
            location_info = ""
            maps_link = None
            
            if device_id in latest_data:
                data = latest_data[device_id]
                lat = data['lat']
                lon = data['long']
                dt = data['datetime']
                maps_link = f"https://www.google.com/maps?q={lat},{lon}"
                location_info = (
                    f"\n📍 *Vị trí gần nhất:*\n"
                    f"🌍 Tọa độ: `{lat}, {lon}`\n"
                    f"⏰ Thời gian: {dt}\n"
                )
            
            alert_message = (
                f"🚨 *CẢNH BÁO KHẨN CẤP!* 🚨\n\n"
                f"⚠️ Thiết bị *{device_id}* đã phát hiện NGÃ!\n"
                f"{location_info}\n"
                f"📞 Liên hệ khẩn cấp nếu cần thiết!"
            )
            
            if maps_link:
                alert_message += f"\n\n[📍 Xem trên Google Maps]({maps_link})"
            
            await telegram_app.bot.send_message(
                chat_id=user_id,
                text=alert_message,
                parse_mode='Markdown',
                disable_web_page_preview=False
            )
            print(f"✅ Đã gửi cảnh báo ngã đến user {user_id} cho thiết bị {device_id}")
    except Exception as e:
        print(f"❌ Lỗi khi gửi cảnh báo: {e}")

# Command: /start
async def start(update: Update, context: ContextTypes.DEFAULT_TYPE):
    user_id = update.effective_user.id
    welcome_message = (
        "🤖 *Chào mừng đến với Bot Quản lý ESP32!*\n\n"
        "Các lệnh có sẵn:\n"
        "/add - Thêm thiết bị ESP32 để theo dõi\n"
        "/speed - Đặt giới hạn tốc độ cho thiết bị\n"
        "/check - Kiểm tra vị trí hiện tại của thiết bị\n"
        "/logout - Đăng xuất khỏi thiết bị\n"
        "/help - Xem hướng dẫn"
    )
    await update.message.reply_text(welcome_message, parse_mode='Markdown')
    
    # Gửi menu lệnh ngay sau đó
    menu_message = (
        "1️⃣ */add* - Nhập ID thiết bị để bắt đầu theo dõi\n"
        "2️⃣ */speed* - Đặt giới hạn tốc độ\n"
        "3️⃣ */check* - Xem vị trí GPS hiện tại\n"
        "4️⃣ */logout* - Ngắt kết nối với thiết bị"
    )
    await update.message.reply_text(menu_message, parse_mode='Markdown')

# Command: /help
async def help_command(update: Update, context: ContextTypes.DEFAULT_TYPE):
    help_text = (
        "📖 *Hướng dẫn sử dụng:*\n\n"
        "1️⃣ */add* - Nhập ID thiết bị để bắt đầu theo dõi\n"
        "   Ví dụ: ESP32_1\n\n"
        "2️⃣ */speed* - Đặt giới hạn tốc độ\n"
        "   Ví dụ: 50 (km/h)\n\n"
        "3️⃣ */check* - Xem vị trí GPS hiện tại\n"
        "   Trả về link Google Maps\n\n"
        "4️⃣ */logout* - Ngắt kết nối với thiết bị"
    )
    await update.message.reply_text(help_text, parse_mode='Markdown')
    
    # Gửi menu lệnh ngay sau đó
    menu_message = (
        "1️⃣ */add* - Nhập ID thiết bị để bắt đầu theo dõi\n"
        "2️⃣ */speed* - Đặt giới hạn tốc độ\n"
        "3️⃣ */check* - Xem vị trí GPS hiện tại\n"
        "4️⃣ */logout* - Ngắt kết nối với thiết bị"
    )
    await update.message.reply_text(menu_message, parse_mode='Markdown')

# Command: /logout
async def logout(update: Update, context: ContextTypes.DEFAULT_TYPE):
    user_id = update.effective_user.id
    if user_id in user_devices:
        device_id = user_devices[user_id]
        del user_devices[user_id]
        
        # Xóa user khỏi danh sách theo dõi device
        if device_id in device_users and user_id in device_users[device_id]:
            device_users[device_id].remove(user_id)
            if not device_users[device_id]:  # Nếu không còn user nào theo dõi
                del device_users[device_id]
        
        await update.message.reply_text(f"✅ Đã đăng xuất khỏi thiết bị: *{device_id}*", parse_mode='Markdown')
    else:
        await update.message.reply_text("❌ Bạn chưa đăng nhập vào thiết bị nào!")

# Command: /add - Bắt đầu conversation để nhập device ID
async def add_device(update: Update, context: ContextTypes.DEFAULT_TYPE):
    await update.message.reply_text(
        "🔧 Vui lòng nhập ID thiết bị ESP32:\n"
        "Ví dụ: ESP32_1\n\n"
        "Gửi /cancel để hủy."
    )
    return WAITING_DEVICE_ID

# Nhận device ID
async def receive_device_id(update: Update, context: ContextTypes.DEFAULT_TYPE):
    user_id = update.effective_user.id
    device_id = update.message.text.strip()
    
    user_devices[user_id] = device_id
    
    # Thêm user vào danh sách theo dõi device
    if device_id not in device_users:
        device_users[device_id] = []
    if user_id not in device_users[device_id]:
        device_users[device_id].append(user_id)
    await update.message.reply_text(
        f"✅ Đã thêm thiết bị: *{device_id}*\n"
        f"Bạn có thể dùng /check để xem vị trí.",
        parse_mode='Markdown'
    )
    
    # Gửi menu lệnh
    menu_message = (
        "1️⃣ */add* - Nhập ID thiết bị để bắt đầu theo dõi\n"
        "2️⃣ */speed* - Đặt giới hạn tốc độ\n"
        "3️⃣ */check* - Xem vị trí GPS hiện tại\n"
        "4️⃣ */logout* - Ngắt kết nối với thiết bị"
    )
    await update.message.reply_text(menu_message, parse_mode='Markdown')
    
    return ConversationHandler.END

# Command: /speed - Đặt giới hạn tốc độ
async def set_speed(update: Update, context: ContextTypes.DEFAULT_TYPE):
    user_id = update.effective_user.id
    
    if user_id not in user_devices:
        await update.message.reply_text("❌ Bạn chưa thêm thiết bị! Dùng /add trước.")
        return ConversationHandler.END
    
    await update.message.reply_text(
        "🚗 Vui lòng nhập giới hạn tốc độ (km/h):\n"
        "Ví dụ: 50\n\n"
        "Gửi /cancel để hủy."
    )
    return WAITING_SPEED

# Nhận giá trị tốc độ
async def receive_speed(update: Update, context: ContextTypes.DEFAULT_TYPE):
    global mqtt_client
    user_id = update.effective_user.id
    device_id = user_devices.get(user_id)
    
    try:
        speed_limit = float(update.message.text.strip())
        
        # Gửi lệnh đến ESP32 qua MQTT theo định dạng CSV: id,speed
        command = f"{device_id},{speed_limit}"
        
        if mqtt_client:
            mqtt_client.publish(TOPIC_PUBLISH, command, qos=1)
            await update.message.reply_text(
                f"✅ Đã đặt giới hạn tốc độ: *{speed_limit} km/h*\n"
                f"Cho thiết bị: *{device_id}*",
                parse_mode='Markdown'
            )
            
            # Gửi menu lệnh
            menu_message = (
                "1️⃣ */add* - Nhập ID thiết bị để bắt đầu theo dõi\n"
                "2️⃣ */speed* - Đặt giới hạn tốc độ\n"
                "3️⃣ */check* - Xem vị trí GPS hiện tại\n"
                "4️⃣ */logout* - Ngắt kết nối với thiết bị"
            )
            await update.message.reply_text(menu_message, parse_mode='Markdown')
        else:
            await update.message.reply_text("❌ Lỗi: MQTT chưa kết nối!")
            
    except ValueError:
        await update.message.reply_text("❌ Giá trị không hợp lệ! Vui lòng nhập số.")
        return WAITING_SPEED
    
    return ConversationHandler.END

# Command: /check - Kiểm tra vị trí
async def check_location(update: Update, context: ContextTypes.DEFAULT_TYPE):
    user_id = update.effective_user.id
    
    if user_id not in user_devices:
        await update.message.reply_text("❌ Bạn chưa thêm thiết bị! Dùng /add trước.")
        return
    
    device_id = user_devices[user_id]
    
    if device_id not in latest_data:
        await update.message.reply_text(
            f"❌ Chưa có dữ liệu từ thiết bị: *{device_id}*\n"
            f"Vui lòng đợi thiết bị gửi dữ liệu.",
            parse_mode='Markdown'
        )
        return
    
    data = latest_data[device_id]
    lat = data['lat']
    lon = data['long']
    speed = data['speed']
    dt = data['datetime']
    
    # Tạo Google Maps link
    maps_link = f"https://www.google.com/maps?q={lat},{lon}"
    
    message = (
        f"📍 *Thông tin thiết bị: {device_id}*\n\n"
        f"🚗 Tốc độ: *{speed} km/h*\n"
        f"🌍 Vĩ độ: `{lat}`\n"
        f"🌍 Kinh độ: `{lon}`\n"
        f"⏰ Thời gian: {dt}\n\n"
        f"[📍 Xem trên Google Maps]({maps_link})"
    )
    
    await update.message.reply_text(message, parse_mode='Markdown', disable_web_page_preview=False)
    
    # Gửi menu lệnh
    menu_message = (
        "1️⃣ */add* - Nhập ID thiết bị để bắt đầu theo dõi\n"
        "2️⃣ */speed* - Đặt giới hạn tốc độ\n"
        "3️⃣ */check* - Xem vị trí GPS hiện tại\n"
        "4️⃣ */logout* - Ngắt kết nối với thiết bị"
    )
    await update.message.reply_text(menu_message, parse_mode='Markdown')

# Cancel conversation
async def cancel(update: Update, context: ContextTypes.DEFAULT_TYPE):
    await update.message.reply_text("❌ Đã hủy thao tác.")
    return ConversationHandler.END

# Tạo MQTT client
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="python_client_" + str(time.time()))
mqtt_client = client  # Lưu vào biến global

# Thiết lập username và password
client.username_pw_set(HIVEMQ_USERNAME, HIVEMQ_PASSWORD)

# Thiết lập TLS/SSL cho kết nối bảo mật
client.tls_set(cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLSv1_2)

# Gán các callback functions
client.on_connect = on_connect
client.on_message = on_message
client.on_publish = on_publish
client.on_disconnect = on_disconnect

# ========== MAIN FUNCTION ==========

async def main():
    global telegram_app, bot_loop

    # Lưu event loop hiện tại để dùng với run_coroutine_threadsafe
    bot_loop = asyncio.get_running_loop()

    # Kết nối MQTT
    print(f"Đang kết nối đến MQTT {HIVEMQ_HOST}:{HIVEMQ_PORT}...")
    client.connect(HIVEMQ_HOST, HIVEMQ_PORT, keepalive=60)
    client.loop_start()
    print("✅ MQTT đã kết nối!")

    # Tạo Telegram Bot Application
    application = Application.builder().token(TELEGRAM_TOKEN).build()
    telegram_app = application  # Lưu vào biến global

    # Thêm handlers
    application.add_handler(CommandHandler("start", start))
    application.add_handler(CommandHandler("help", help_command))
    application.add_handler(CommandHandler("logout", logout))
    application.add_handler(CommandHandler("check", check_location))

    # Conversation handler cho /add
    add_conv_handler = ConversationHandler(
        entry_points=[CommandHandler("add", add_device)],
        states={
            WAITING_DEVICE_ID: [MessageHandler(filters.TEXT & ~filters.COMMAND, receive_device_id)],
        },
        fallbacks=[CommandHandler("cancel", cancel)],
    )
    application.add_handler(add_conv_handler)

    # Conversation handler cho /speed
    speed_conv_handler = ConversationHandler(
        entry_points=[CommandHandler("speed", set_speed)],
        states={
            WAITING_SPEED: [MessageHandler(filters.TEXT & ~filters.COMMAND, receive_speed)],
        },
        fallbacks=[CommandHandler("cancel", cancel)],
    )
    application.add_handler(speed_conv_handler)

    # Chạy bot dùng API thấp hơn để tránh conflict event loop với nest_asyncio
    print("✅ Telegram Bot đang chạy...")
    print("Bot đang lắng nghe tin nhắn. Nhấn Ctrl+C để thoát.")
    async with application:
        await application.initialize()
        await application.start()
        await application.updater.start_polling()
        try:
            # Giữ chương trình chạy cho đến khi bị hủy
            await asyncio.Event().wait()
        except asyncio.CancelledError:
            pass
        finally:
            await application.updater.stop()
            await application.stop()

if __name__ == "__main__":
    import asyncio
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n⏹ Đang dừng bot...")
        client.loop_stop()
        client.disconnect()
        print("✅ Đã thoát!")
