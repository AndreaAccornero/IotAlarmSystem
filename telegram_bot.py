import asyncio
import os
import json
import aiohttp
import nest_asyncio
import logging
from dotenv import load_dotenv
from telegram import Update, InlineKeyboardButton, InlineKeyboardMarkup
from telegram.ext import (
    Application, CallbackContext, CommandHandler, MessageHandler, filters,
    CallbackQueryHandler, ConversationHandler
)
from telegram.error import BadRequest

# Carica le variabili d'ambiente
load_dotenv(".env")
TELEGRAM_BOT_TOKEN = os.getenv("TELEGRAM_BOT_TOKEN")
FLASK_SERVER_URL = os.getenv("FLASK_SERVER_URL")

# Stati della conversazione
CHOOSING_ALARM_ID, CHOOSING_TIME, CHOOSING_FREQUENCY, MODIFY_ALARM, REMOVE_ALARM = range(5)

# Configura logging
logging.basicConfig(
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s', level=logging.INFO
)

# ===================== FUNZIONI HELPER =====================

def get_chat_id(update: Update) -> int:
    """Recupera il chat_id dall'update (sia per callback che per messaggio diretto)."""
    if update.callback_query and update.callback_query.message:
        return update.callback_query.message.chat_id
    elif update.effective_chat:
        return update.effective_chat.id
    return None

async def safe_delete(message):
    """Prova a cancellare il messaggio, se non esiste logga un warning."""
    try:
        await message.delete()
    except BadRequest as e:
        logging.warning(f"safe_delete: Impossibile cancellare il messaggio: {e}")

def back_to_menu_keyboard():
    """Restituisce un inline keyboard con il pulsante per tornare al menu principale."""
    return InlineKeyboardMarkup([[InlineKeyboardButton("🔙 Torna al Menu", callback_data="main_menu")]])

# ===================== COMANDO /START E MENU PRINCIPALE =====================

async def start(update: Update, context: CallbackContext) -> None:
    """Comando /start: mostra il menu principale."""
    await main_menu(update, context)

async def main_menu(update: Update, context: CallbackContext) -> None:
    """Mostra il menu principale con tutte le opzioni."""
    chat_id = get_chat_id(update)
    keyboard = [
        [InlineKeyboardButton("⏰ Aggiungi Sveglia", callback_data='set_alarm')],
        [InlineKeyboardButton("✏️ Modifica Sveglia", callback_data='modify_alarm')],
        [InlineKeyboardButton("❌ Rimuovi Sveglia", callback_data='remove_alarm')],
        [InlineKeyboardButton("❌ Rimuovi Tutte le Sveglie", callback_data='remove_all_alarms')],
        [InlineKeyboardButton("🛑 Ferma Allarme", callback_data='stop_alarm')],
        [InlineKeyboardButton("📋 Lista Sveglie", callback_data='list_alarms')]
    ]
    reply_markup = InlineKeyboardMarkup(keyboard)
    
    if update.callback_query:
        await update.callback_query.answer()
        if update.callback_query.message:
            await safe_delete(update.callback_query.message)
        await context.bot.send_message(
            chat_id=chat_id,
            text="📌 **Gestisci le tue sveglie:**",
            reply_markup=reply_markup
        )
    elif update.message:
        await update.message.reply_text("📌 **Gestisci le tue sveglie:**", reply_markup=reply_markup)

# ===================== CREAZIONE DELLA SVEGLIA =====================

async def start_setting_alarm(update: Update, context: CallbackContext) -> int:
    """Avvia la creazione della sveglia impostando l'azione 'create'."""
    chat_id = get_chat_id(update)
    await update.callback_query.answer()
    if update.callback_query.message:
        await safe_delete(update.callback_query.message)
    context.user_data["action"] = "create"
    # Chiediamo l'ID della sveglia
    keyboard = [[InlineKeyboardButton("🔙 Torna al Menu", callback_data="cancel")]]
    reply_markup = InlineKeyboardMarkup(keyboard)
    await context.bot.send_message(
        chat_id=chat_id,
        text="✍️ **Scrivi un ID per la nuova sveglia nella chat:**",
        reply_markup=reply_markup
    )
    return CHOOSING_ALARM_ID

async def receive_alarm_id(update: Update, context: CallbackContext) -> int:
    """Salva l'ID della sveglia e chiede di inserire l'orario manualmente."""
    user_data = context.user_data
    user_data["alarm_id"] = update.message.text.strip()
    await update.message.reply_text(
        "⏳ **Scrivi l'orario della sveglia (formato HH:MM):**",
        reply_markup=back_to_menu_keyboard()
    )
    return CHOOSING_TIME

async def receive_time(update: Update, context: CallbackContext) -> int:
    """Salva l'orario inserito e chiede la frequenza."""
    user_data = context.user_data
    user_data["time"] = update.message.text.strip()
    await update.message.reply_text(
        "📅 **Scrivi la frequenza della sveglia (es. everyday, once, weekdays, weekends, every_monday, ecc.):**",
        reply_markup=back_to_menu_keyboard()
    )
    return CHOOSING_FREQUENCY

async def receive_frequency(update: Update, context: CallbackContext) -> int:
    """Salva la frequenza e completa il flusso (creazione o modifica)."""
    user_data = context.user_data
    user_data["frequency"] = update.message.text.strip()
    chat_id = update.message.chat_id
    if user_data.get("action") == "create":
        payload = {
            "alarm_id": user_data["alarm_id"],
            "alarm_time": user_data["time"],
            "alarm_frequency": user_data["frequency"]
        }
        url = f"{FLASK_SERVER_URL}/set_new_alarm"
        async with aiohttp.ClientSession() as session:
            async with session.post(url, json=payload) as resp:
                if resp.status == 400:
                    error_data = await resp.json()
                    await context.bot.send_message(
                        chat_id=chat_id,
                        text=f"❌ Errore: {error_data.get('message', 'ID già esistente.')}",
                        reply_markup=back_to_menu_keyboard()
                    )
                    return ConversationHandler.END
                else:
                    await resp.json()
        await context.bot.send_message(
            chat_id=chat_id,
            text=(
                f"✅ **Sveglia impostata con successo!**\n"
                f"🕒 Orario: {user_data['time']}\n"
                f"📅 Frequenza: {user_data['frequency']}"
            ),
            reply_markup=back_to_menu_keyboard()
        )
        return ConversationHandler.END

    elif user_data.get("action") == "modify":
        payload = {
            "alarm_time": user_data["time"],
            "alarm_frequency": user_data["frequency"]
        }
        url = f"{FLASK_SERVER_URL}/update_alarm/{user_data['alarm_id']}"
        async with aiohttp.ClientSession() as session:
            async with session.put(url, json=payload) as resp:
                await resp.json()
        await context.bot.send_message(
            chat_id=chat_id,
            text=(
                f"✅ **Sveglia {user_data['alarm_id']} modificata con successo!**\n"
                f"🕒 Nuovo orario: {user_data['time']}\n"
                f"📅 Nuova frequenza: {user_data['frequency']}"
            ),
            reply_markup=back_to_menu_keyboard()
        )
        return ConversationHandler.END

    return ConversationHandler.END

# ===================== MODIFICA DELLA SVEGLIA =====================

async def modify_alarm(update: Update, context: CallbackContext) -> int:
    """Avvia il flusso di modifica impostando l'azione 'modify'."""
    chat_id = get_chat_id(update)
    await update.callback_query.answer()
    if update.callback_query.message:
        await safe_delete(update.callback_query.message)
    context.user_data["action"] = "modify"
    await context.bot.send_message(
        chat_id=chat_id,
        text="✏️ **Scrivi l'ID della sveglia da modificare:**",
        reply_markup=back_to_menu_keyboard()
    )
    return MODIFY_ALARM

async def modify_alarm_id(update: Update, context: CallbackContext) -> int:
    """Salva l'ID della sveglia da modificare e chiede il nuovo orario."""
    user_data = context.user_data
    user_data["alarm_id"] = update.message.text.strip()
    await update.message.reply_text(
        "⏳ **Scrivi il nuovo orario della sveglia (formato HH:MM):**",
        reply_markup=back_to_menu_keyboard()
    )
    return CHOOSING_TIME

# ===================== RIMOZIONE DI UNA SVEGLIA =====================

async def remove_alarm(update: Update, context: CallbackContext) -> int:
    """Avvia il flusso per rimuovere una sveglia (singola)."""
    chat_id = get_chat_id(update)
    await update.callback_query.answer()
    if update.callback_query.message:
        await safe_delete(update.callback_query.message)
    await context.bot.send_message(
        chat_id=chat_id,
        text="❌ **Scrivi l'ID della sveglia da eliminare:**",
        reply_markup=back_to_menu_keyboard()
    )
    return REMOVE_ALARM

async def confirm_removal(update: Update, context: CallbackContext) -> int:
    """Conferma l'eliminazione di una sveglia e controlla se l'ID esiste sul server."""
    alarm_id = update.message.text.strip()
    url = f"{FLASK_SERVER_URL}/remove_alarm/{alarm_id}"
    async with aiohttp.ClientSession() as session:
        async with session.delete(url) as resp:
            data = await resp.json()
            if resp.status == 400:
                # L'endpoint ha restituito un errore (es. ID non esistente)
                await update.message.reply_text(
                    f"❌ Errore: {data.get('message', 'Sveglia non trovata.')}",
                    reply_markup=back_to_menu_keyboard()
                )
            else:
                await update.message.reply_text(
                    f"✅ **Sveglia {alarm_id} eliminata con successo!**",
                    reply_markup=back_to_menu_keyboard()
                )
    return ConversationHandler.END


# ===================== RIMOZIONE DI TUTTE LE SVEGLIE =====================

async def remove_all_alarms(update: Update, context: CallbackContext) -> None:
    """Elimina tutte le sveglie chiamando l'endpoint dedicato."""
    chat_id = get_chat_id(update)
    await update.callback_query.answer()
    url = f"{FLASK_SERVER_URL}/remove_all_alarms"
    async with aiohttp.ClientSession() as session:
        async with session.delete(url) as resp:
            await resp.json()
    await context.bot.send_message(
        chat_id=chat_id,
        text="✅ **Tutte le sveglie sono state eliminate con successo!**",
        reply_markup=back_to_menu_keyboard()
    )

# ===================== STOP DELL'ALLARME =====================

async def stop_alarm(update: Update, context: CallbackContext) -> None:
    """Ferma l'allarme attivo chiamando l'endpoint."""
    chat_id = get_chat_id(update)
    await update.callback_query.answer()
    url = f"{FLASK_SERVER_URL}/update_stop_alarm"
    payload = {"stop_alarm": "true"}
    async with aiohttp.ClientSession() as session:
        async with session.post(url, json=payload) as resp:
            await resp.json()
    await context.bot.send_message(
        chat_id=chat_id,
        text="🛑 **Allarme fermato con successo!**",
        reply_markup=back_to_menu_keyboard()
    )

# ===================== LISTA DELLE SVEGLIE =====================

async def list_alarms(update: Update, context: CallbackContext) -> None:
    """Recupera e mostra la lista delle sveglie esistenti."""
    chat_id = get_chat_id(update)
    await update.callback_query.answer()
    url = f"{FLASK_SERVER_URL}/alarms"
    async with aiohttp.ClientSession() as session:
        async with session.get(url) as resp:
            result = await resp.json()
            status = resp.status

    if status == 200:
        alarms_list = result.get("alarms", [])
        if not alarms_list:
            message = "⚠️ **Nessuna sveglia impostata.**"
        else:
            message = "⏰ **Sveglie attive:**\n\n"
            for alarm in alarms_list:
                message += (
                    f"📌 ID: `{alarm['alarm_id']}` - "
                    f"🕒 {alarm['alarm_time']} - "
                    f"📅 {alarm['alarm_frequency']}\n"
                )
    else:
        message = "❌ **Errore nel recupero delle sveglie.**"
    await context.bot.send_message(
        chat_id=chat_id,
        text=message,
        reply_markup=back_to_menu_keyboard()
    )

# ===================== CANCELLAZIONE =====================

async def cancel(update: Update, context: CallbackContext) -> int:
    """Annulla l'operazione in corso e torna al menu principale."""
    chat_id = get_chat_id(update)
    if update.callback_query:
        await update.callback_query.answer()
        if update.callback_query.message:
            await safe_delete(update.callback_query.message)
        await context.bot.send_message(
            chat_id=chat_id,
            text="Operazione annullata.",
            reply_markup=back_to_menu_keyboard()
        )
    elif update.message:
        await update.message.reply_text("Operazione annullata.", reply_markup=back_to_menu_keyboard())
    await main_menu(update, context)
    return ConversationHandler.END

# ===================== HANDLER DI ERRORE (FACOLTATIVO) =====================

async def error_handler(update: object, context: CallbackContext) -> None:
    logging.error(msg="Exception while handling an update:", exc_info=context.error)

# ===================== AVVIO DEL BOT =====================

async def main():
    application = Application.builder().token(TELEGRAM_BOT_TOKEN).build()

    conv_handler = ConversationHandler(
        entry_points=[
            CallbackQueryHandler(start_setting_alarm, pattern="^set_alarm$"),
            CallbackQueryHandler(modify_alarm, pattern="^modify_alarm$"),
            CallbackQueryHandler(remove_alarm, pattern="^remove_alarm$")
        ],
        states={
            CHOOSING_ALARM_ID: [MessageHandler(filters.TEXT & ~filters.COMMAND, receive_alarm_id)],
            MODIFY_ALARM: [MessageHandler(filters.TEXT & ~filters.COMMAND, modify_alarm_id)],
            CHOOSING_TIME: [MessageHandler(filters.TEXT & ~filters.COMMAND, receive_time)],
            CHOOSING_FREQUENCY: [MessageHandler(filters.TEXT & ~filters.COMMAND, receive_frequency)],
            REMOVE_ALARM: [MessageHandler(filters.TEXT & ~filters.COMMAND, confirm_removal)]
        },
        fallbacks=[CallbackQueryHandler(cancel, pattern="^cancel$")]
    )

    application.add_handler(conv_handler)
    application.add_handler(CommandHandler("start", start))
    application.add_handler(CallbackQueryHandler(main_menu, pattern="^main_menu$"))
    application.add_handler(CallbackQueryHandler(stop_alarm, pattern="^stop_alarm$"))
    application.add_handler(CallbackQueryHandler(list_alarms, pattern="^list_alarms$"))
    application.add_handler(CallbackQueryHandler(remove_all_alarms, pattern="^remove_all_alarms$"))
    application.add_handler(CallbackQueryHandler(cancel, pattern="^cancel$"))

    application.add_error_handler(error_handler)

    await application.run_polling()

if __name__ == "__main__":
    nest_asyncio.apply()
    asyncio.run(main())
