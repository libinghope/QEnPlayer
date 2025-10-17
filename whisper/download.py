import whisper

# 下载并加载指定模型（如果没有会自动下载）
model = whisper.load_model("medium.en")

print("模型已下载并缓存到：~/.cache/whisper")
