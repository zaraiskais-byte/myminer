#!/usr/bin/env python3
"""Caesar CZR AI Code Reviewer - uses free HuggingFace inference API"""
import os, sys, json, urllib.request, glob

API_URL = "https://api-inference.huggingface.co/models/bigcode/starcoder2-3b"
TOKEN = os.environ.get("HF_TOKEN", "")

def review_file(path):
    with open(path) as f:
        content = f.read()[:3000]
    prompt = f"// Review this C++ code for bugs and security issues:\n{content}\n// Issues:"
    req = urllib.request.Request(API_URL,
        data=json.dumps({"inputs": prompt}).encode(),
        headers={"Authorization": f"Bearer {TOKEN}",
                 "Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            result = json.loads(r.read())
            return result[0].get("generated_text", "")[:500]
    except Exception as e:
        return f"(AI unavailable: {e})"

def main():
    files = sys.argv[1:] or glob.glob("src/*.cpp")[:2]
    for f in files:
        print(f"\n=== AI Review: {f} ===")
        print(review_file(f))

if __name__ == "__main__":
    main()
