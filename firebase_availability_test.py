import firebase_admin
from firebase_admin import credentials
from firebase_admin import firestore
import time
import uuid 
import schedule 

SERVICE_ACCOUNT_KEY_PATH = "./CD/serviceAccountKey.json"

db = None 

def initialize_firebase():
    """Menginisialisasi Firebase Admin SDK."""
    global db
    try:
        if not firebase_admin._apps: 
            cred = credentials.Certificate(SERVICE_ACCOUNT_KEY_PATH)
            firebase_admin.initialize_app(cred)
        db = firestore.client()
        print("Firebase Admin SDK berhasil diinisialisasi untuk Firestore.")
        return True
    except Exception as e:
        print(f"Gagal menginisialisasi Firebase Admin SDK: {e}")
        return False

if not initialize_firebase():
    exit()

AVAILABILITY_COLLECTION_NAME = "firebase_availability_test" 
HEARTBEAT_INTERVAL_SECONDS = 60  
TOTAL_TEST_DURATION_HOURS = 24

successful_heartbeats = 0
failed_heartbeats = 0
test_start_time_availability = None

def send_firestore_heartbeat():
    """Mengirim heartbeat ke Firestore."""
    global successful_heartbeats, failed_heartbeats
    
    current_timestamp_val = time.time()
    data = {"client_timestamp": current_timestamp_val, "status": "alive"}
    
    try:
        doc_ref = db.collection(AVAILABILITY_COLLECTION_NAME).document(f"heartbeat_{int(current_timestamp_val)}")
        
        data_with_timestamp = data.copy()
        data_with_timestamp['server_timestamp'] = firestore.SERVER_TIMESTAMP
        
        doc_ref.set(data_with_timestamp)
        print(f"[{time.ctime()}] Heartbeat ke Firestore berhasil.")
        successful_heartbeats += 1
    except Exception as e:
        print(f"[{time.ctime()}] GAGAL mengirim heartbeat ke Firestore: {e}")
        failed_heartbeats += 1

def print_availability_stats():
    """Mencetak statistik ketersediaan."""
    total_attempts = successful_heartbeats + failed_heartbeats
    availability_percentage = 0
    if total_attempts > 0:
        availability_percentage = (successful_heartbeats / total_attempts) * 100
    
    elapsed_time_seconds = time.time() - test_start_time_availability if test_start_time_availability else 0
    elapsed_time_hours = elapsed_time_seconds / 3600

    print(f"\n--- Statistik Ketersediaan Firestore ---")
    print(f"Total percobaan heartbeat: {total_attempts}") # nanti ganti
    print(f"Berhasil: {successful_heartbeats}")
    print(f"Gagal: {failed_heartbeats}")
    print(f"Ketersediaan: {availability_percentage:.3f}%")

def test_firestore_availability():
    global test_start_time_availability
    print(f"Memulai pengujian ketersediaan Firestore")
    print(f"Heartbeat akan dikirim setiap {HEARTBEAT_INTERVAL_SECONDS} detik.")
    
    test_start_time_availability = time.time()
    
    schedule.every(HEARTBEAT_INTERVAL_SECONDS).seconds.do(send_firestore_heartbeat)

    schedule.every(1).hours.do(print_availability_stats)

    end_loop_time = test_start_time_availability + (TOTAL_TEST_DURATION_HOURS * 3600)
    try:
        while time.time() < end_loop_time:
            schedule.run_pending()
            time.sleep(1) 
    except KeyboardInterrupt:
        print("\nPengujian dihentikan secara manual.")
    finally:
        print("\nPengujian ketersediaan Firestore selesai.")
        print_availability_stats()

if __name__ == "__main__":
    if db:
        test_firestore_availability()