from pylibdmtx.pylibdmtx import encode
from PIL import Image
import os
import sys

def main():
    if len(sys.argv) < 2:
        print("Ошибка: не передан серийный номер")
        sys.exit(1)
        
    serialNumber = sys.argv[1]
    
    # Создаем папку datamatrix, если ее нет
    dir_path = "markings"
    if not os.path.exists(dir_path):
        os.makedirs(dir_path)

    try:
        # Кодируем серийный номер
        data = serialNumber.encode('utf-8')
        encoded = encode(data)
        
        # Создаем изображение
        img = Image.frombytes('RGB', (encoded.width, encoded.height), encoded.pixels)
        
        # Сохраняем в папку datamatrix
        file_path = os.path.join(dir_path, f"{serialNumber}.png")
        img.save(file_path)
        
        # Выводим путь, чтобы С++ мог его прочитать (опционально)
        print(file_path)
        
    except Exception as e:
        print(f"Ошибка генерации: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
