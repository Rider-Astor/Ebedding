from zhipuai import ZhipuAI

from flask import Flask, jsonify, request
import requests
app = Flask(__name__)

# from rank_bm25 import BM25Okapi
# import jieba
# content_words = [jieba.lcut(part['content']) for part in course_data]
# bm25 = BM25Okapi(content_words)

# course_data = [
#     {'id': idx, 'name': name, 'content': content}
#     for idx, (name, content) in enumerate(cursor.fetchall())
#     if name is not None
# ]

##连接到zhipuai大模型
api_key = "5f334b3a9da2faba23cde717451c9267.ZKRI5SgQRqsbvhse"
client = ZhipuAI(api_key=api_key)

standardInput = {"role" : "user", "content" : ""}
systemInput = {"role" : "system", "content" : """你是一个对话机器人，回应用户的请求，请控制你的对话长度小于150个汉字。"""}
Messages = []##prompt into LLM

#以下是功能函数
@app.route('/api/python-service', methods=['POST'])
def model_function():

    questions = request.get_json()#这里假设questions直接获得是问题

    Messages.append(systemInput)
    standardInput["content"] = questions["question"]
    Messages.append(standardInput)

    response = client.chat.completions.create(
      model = "glm-4-flash",
      messages = Messages
    )
    return jsonify({'response': response.choices[0].message.content})

if __name__ == '__main__':
    app.run(host='0.0.0.0',port=5001)