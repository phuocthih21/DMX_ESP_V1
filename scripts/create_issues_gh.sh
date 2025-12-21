#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   export GITHUB_REPO="phuocthih21/DMX_ESP_V1"
#   ./create_issues_gh.sh

REPO="${GITHUB_REPO:-}"
if [ -z "$REPO" ]; then
  echo "ERROR: Please set GITHUB_REPO environment variable (owner/repo)." >&2
  exit 1
fi

# Iterate over markdown files in .github/issues
for f in .github/issues/*.md; do
  [ -e "$f" ] || continue

  echo "------------------------------------------------"
  echo "Processing file: $f"
  
  # 1. Lấy Tiêu đề
  title=$(sed -n '1s/^# \+//p' "$f")
  
  # 2. Lấy Labels (Chỉ lấy chữ cái, số và dấu gạch ngang/phẩy - loại bỏ ký tự lạ)
  labels=$(grep -m1 '^Labels:' "$f" | sed 's/^Labels:[[:space:]]*//' | tr -d "\"\'") || labels=""
  
  # 3. Lấy Body (Nội dung)
  body=$(awk 'NR>1 && !/^Labels:/ && !/^Assignees:/{print}' "$f")

  # --- THAY ĐỔI QUAN TRỌNG TẠI ĐÂY ---
  # Không đọc Assignee từ file nữa để tránh lỗi dấu nháy hoặc sai user.
  # Gán cứng luôn cho @me (Chính bạn).
  echo "Force assigning to: @me"
  
  # Tạo mảng tham số
  cmd_args=( --repo "$REPO" --title "$title" --body "$body" --assignee "@me" )

  # Thêm Label nếu có
  if [ -n "$labels" ]; then
    cmd_args+=( --label "$labels" )
  fi

  echo "Creating issue: $title"
  
  # Chạy lệnh
  gh issue create "${cmd_args[@]}" || {
    echo "❌ Failed to create issue: $title" >&2
  }
  
  sleep 1
done

echo "✅ Completed. Verify on GitHub."