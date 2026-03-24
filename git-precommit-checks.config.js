module.exports = {
  display: {
    notifications: true,
    offendingContent: true,
    rulesSummary: false,
    shortStats: true,
    verbose: false,
  },
  rules: [
    {
      message: 'Aurais-tu oublié de terminer certaines tâches ?  Aurais-tu une issue à ouvrir pour traiter cette tache plus tard ?',
      nonBlocking: true,
      regex: /(?:FIXME|TODO)/,
    },
    {
      message: 'Tu as des marqueurs de conflits qui traînent dans ton code',
      regex: /^[<>|=]{4,}/m,
    },
    {
      message:
        'Arrêt du commit : tu as renseigné des choses qui ne doivent pas être commitées !',
      regex: /do[\s]not[\s]commit/i,
    },
  ],
}
