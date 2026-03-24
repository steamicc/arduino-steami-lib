module.exports = {
    pattern: "^(main)$|^(feat|fix|docs|tooling|ci|test|style|chore|refactor)\/([a-z0-9]+-)*[a-z0-9]+$|^release\/v([0-9]+)\\.([0-9]+)\\.([0-9]+)([a-z0-9-]*)$",
    errorMsg: "🤨 La branche que tu essaies de pusher ne respecte pas nos conventions, tu peux la renommer avec `git branch -m <nom-actuel> <nouveau-nom>`",
}
